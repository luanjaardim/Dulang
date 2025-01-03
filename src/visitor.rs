use crate::{grammar::{Node, ASTNode}, tokenizer::{Token, TokenType}};

use ExprType::*;
#[derive(Debug, Clone)]
pub enum ExprType {
    // Types
    Int{ bits: usize, signed: bool }, Real(usize), Char, Bool,

    // Compounded types
    FnType(Vec<ExprType>), UnionType(Vec<ExprType>), TupleType(Vec<ExprType>),

    Void, Unknown
}
impl ExprType {
    fn expr_type_eq(f: &ExprType, s: &ExprType, strict_cmp: bool) -> bool {
        match (f, s) {
            (Char, Char)
            | (Bool, Bool)
            | (Void, Void) => true,
              (_, Unknown) if !strict_cmp => true,
              (Unknown, _) if !strict_cmp => true,

            (Real(b1), Real(b2)) if b1 == b2 => true,
            (Int { bits: b1, signed: s1 }, Int { bits: b2, signed: s2 }) if b1 == b2 && s1 == s2 => true,

            (FnType(l1), FnType(l2))
            | (UnionType(l1), UnionType(l2))
            | (TupleType(l1), TupleType(l2)) => {
               if l1.len() != l2.len() { return false }
               l1.iter().enumerate().all(|(i, e)| Self::expr_type_eq(e, &l2[i], strict_cmp))
            },
            _ => false,
        }
    }
    /// NOTE: only use this function when you are sure both types are equal,
    /// this function will use every type known from both ExprType to build the
    /// final type, filling every Unknown possible
    fn final_type(self, s: &Self) -> Self {
        let fill_unknown = |l1: Vec<ExprType>, l2: &Vec<ExprType>| {
            l1.into_iter().zip(l2.iter()).map(|(e1, e2)| if e1 != Unknown { e1 } else { e2.clone() }).collect()
        };
        if let Unknown = self { return s.clone() }
        match (self, s) {
            (FnType(l1), FnType(l2)) => FnType(fill_unknown(l1, l2)),
            (UnionType(l1), UnionType(l2)) => UnionType(fill_unknown(l1, l2)),
            (TupleType(l1), TupleType(l2)) => TupleType(fill_unknown(l1, l2)),
            (t, _) => t,
        }
    }
    fn get_nth_inner_type(&self, nth: usize) -> Self {
        match self {
            FnType(l) | UnionType(l) | TupleType(l) => l[nth].clone(),
            _ => panic!("Cannot get a inner type of a non compound type"),
        }
    }
    fn get_fn_return_type(&self) -> Self {
        match self {
            FnType(inner_types) => inner_types.last().unwrap().clone(),
            _ => panic!("Cannot get the function return type of a non function type"),
        }
    }

    // FIX: is unknown should verify its inner types too, if any is unknown the type is unknown
    fn is_unknown(&self) -> bool { if let Unknown = self { true } else { false }}
}

/// Comparing between ExprType with '==' or '!=' is not a strict
/// comparison, Unknown values are equal to any other type, to compare it preciselly
/// pass 'true' as the 'strict_cmp' argument
impl PartialEq for ExprType {
    fn eq(&self, other: &Self) -> bool {
        Self::expr_type_eq(self, other, false)
    }
    fn ne(&self, other: &Self) -> bool {
        !Self::expr_type_eq(self, other, false)
    }
}

impl From<&Node> for ExprType {
    fn from(value: &Node) -> Self {
        match &**value {
            ASTNode::Type { t: TokenType::I32, inner_types } if inner_types.is_empty() => Int { bits: 32, signed: true },
            ASTNode::Type { t: TokenType::U32, inner_types } if inner_types.is_empty() => Int { bits: 32, signed: false },
            ASTNode::Type { t: TokenType::F32, inner_types } if inner_types.is_empty() => Real(32),
            ASTNode::Type { t: TokenType::F64, inner_types } if inner_types.is_empty() => Real(64),
            ASTNode::Type { t: TokenType::Bool, inner_types } if inner_types.is_empty() => Bool,
            ASTNode::Type { t: TokenType::Char, inner_types } if inner_types.is_empty() => Char,
            ASTNode::Type { t: TokenType::Void, inner_types } if inner_types.is_empty() => Void,
            ASTNode::Type { t: TokenType::FnType, inner_types } => FnType(inner_types.iter().map(|it| ExprType::from(it)).collect()),
            ASTNode::Type { t: TokenType::UnionType, inner_types } => UnionType(inner_types.iter().map(|it| ExprType::from(it)).collect()),
            ASTNode::Type { t: TokenType::TupleType, inner_types } => TupleType(inner_types.iter().map(|it| ExprType::from(it)).collect()),
            _ => Unknown,
        }
    }
}

#[derive(Debug)]
struct Var {
    v: Token,
    t: ExprType,
}

#[derive(Debug)]
enum ScopeAttr {
    GlobScope,
    FuncScope {
        ret_type: ExprType,
        args_len: usize,
    },
    CondScope,
    LoopScope {
        label: String,
    }
}

#[derive(Debug)]
pub struct Scope {
    attrs: ScopeAttr,
    scopes: Vec<Scope>,
    vars: Vec<Var>,
    scp_father: *const Scope,
}

impl Scope {
    fn find_var(scp: *const Scope, var_name: &str) -> Option<ExprType> {
        unsafe {
            if scp.is_null() { return None }

            let scp_ref = &*scp;
            for var in scp_ref.vars.iter().rev() {
                if var.v.text.as_str() == var_name {
                    return Some(var.t.clone())
                }
            }
            Scope::find_var(scp_ref.scp_father, var_name)
        }
    }

    fn update_var_type(scp: *mut Scope, t: ExprType, var_name: &str) {
        unsafe {
            if scp.is_null() { return }

            let scp_ref = &mut *scp;
            for i in (0..scp_ref.vars.len()).rev() {
                if scp_ref.vars[i].v.text.as_str() == var_name {
                    scp_ref.vars[i].t = t;
                    return
                }
            }
            Scope::update_var_type(scp_ref.scp_father as *mut Scope, t, var_name);
        }
    }

    fn is_inside_loop(&self) -> bool {
        if let ScopeAttr::LoopScope { .. } = self.attrs { true }
        else {
            unsafe { self.scp_father.as_ref().map_or(false, |f| f.is_inside_loop()) }
        }
    }

    fn get_cur_func_scp(&self) -> Option<&Self> {
        if let ScopeAttr::FuncScope { .. } = self.attrs { Some(self) }
        else {
            unsafe { self.scp_father.as_ref().map_or(None, |f| f.get_cur_func_scp()) }
        }
    }

    fn set_cur_func_ret_type(scp: *mut Scope, t: ExprType) {
        unsafe {
            if scp.is_null() { return }

            let scp_ref = &mut *scp;
            if let ScopeAttr::FuncScope { args_len, .. } = scp_ref.attrs {
                scp_ref.attrs = ScopeAttr::FuncScope { ret_type: t, args_len }
            } else {
                Scope::set_cur_func_ret_type(scp_ref.scp_father as *mut Scope, t) 
            }
        }
    }
}

#[derive(Debug)]
pub enum VisitorError {
    VariableNotDeclared(String),
    MismatchedTypes(ExprType, ExprType),
    GeneralError(String),
    NotImplemented(ASTNode)
}

pub struct Visitor {
    ast: Vec<Node>,
    pub glob_scope: Option<Scope>,
}

impl Visitor {
    pub fn new(parsed_ast:  Vec<Node>) -> Self {
        Visitor { ast: parsed_ast, glob_scope: None }
    }
    pub fn traverse(&mut self) -> Result<(), std::io::Error> {
        use std::io::{Error, ErrorKind};
        let ast = std::mem::take(&mut self.ast);
        let mut global_scope = Scope { attrs: ScopeAttr::GlobScope, scopes: vec![], vars: vec![], scp_father: std::ptr::null() };
        for n in &ast {
            self.visit(&mut global_scope, Void, n).map_err(|e| Error::new(ErrorKind::InvalidInput, format!("Visitor Error: {e:?}")))?;
        }
        self.ast = ast;
        self.glob_scope = Some(global_scope);
        Ok(())
    }

    fn visit(&mut self, scope: &mut Scope, expected_type: ExprType, node: &Node) -> Result<ExprType, VisitorError> {
        match &**node {
            ASTNode::Assign { var: (v, var_type), expr: expression } => {
                let expected_type = var_type.as_ref().map_or(Unknown, |t| ExprType::from(t));
                let expression_type = self.visit(scope, expected_type, expression)?;
                scope.vars.push(Var {
                    v: v.clone(),
                    t: expression_type,
                });
                Ok(Void)
            },
            ASTNode::Func { args, ret, body } => {
                let ret_type = ret.as_ref().map_or(Unknown, |t| ExprType::from(t)).clone();
                let fn_type = FnType(
                    if !args.is_empty() {
                        (0..args.len())
                            .map(|i| args[i].1.as_ref().map_or(Unknown, |t| ExprType::from(t)).clone())
                            .chain([ret_type])
                            .collect()
                    } else {
                        vec![Void, ret_type]
                    });
                if fn_type == expected_type {
                    let known_func_type = fn_type.final_type(&expected_type);

                    scope.scopes.push(Scope {
                        attrs: ScopeAttr::FuncScope {
                            ret_type: known_func_type.get_fn_return_type(),
                            args_len: args.len(),
                        },
                        scopes: vec![],
                        vars: (0..args.len()).map(|i| Var {
                                  v: args[i].0.clone(),
                                  t: known_func_type.get_nth_inner_type(i),
                              }).collect(),
                        scp_father: scope,
                    });
                    for node in body {
                        // TODO: Void may not be the best type to be expected, but for statements it's fine
                        self.visit(scope.scopes.last_mut().unwrap(), Void, node)?;
                    }
                    // TODO: recalculate the type of the function after the body is visited
                    Ok(known_func_type)
                } else {
                    Err(VisitorError::MismatchedTypes(fn_type, expected_type))
                }
            },
            ASTNode::Loop { cond, body } => {
                if let Some(cond_expr) = cond {
                    self.visit(scope, Bool, cond_expr)?;
                }
                scope.scopes.push(Scope {
                    // TODO: implement label declaration for loops
                    attrs: ScopeAttr::LoopScope { label: String::new() },
                    scopes: vec![],
                    vars: vec![],
                    scp_father: scope,
                });
                for node in body {
                    self.visit(scope.scopes.last_mut().unwrap(), Void, node)?;
                }
                Ok(Void)
            },
            ASTNode::Conditional { cond, body, next } => {
                if let Some(cond_expr) = cond {
                    self.visit(scope, Bool, cond_expr)?;
                }
                scope.scopes.push(Scope {
                    attrs: ScopeAttr::CondScope,
                    scopes: vec![],
                    vars: vec![],
                    scp_father: scope,
                });
                for node in body {
                    self.visit(scope.scopes.last_mut().unwrap(), Void, node)?;
                }
                if let Some(n) = next {
                    self.visit(scope, Bool, n)?;
                }
                Ok(Void)
            }
            ASTNode::FlowChange(tk_type, ret) => {
                match tk_type {
                    TokenType::Back => {
                        let ret_type = if let Some(fn_scope) = scope.get_cur_func_scp() {
                            if let ScopeAttr::FuncScope { ret_type, .. } = &fn_scope.attrs {
                                ret_type.clone()
                            } else { unreachable!("get_current_function_scope only returns a scope that is from a function") }
                        } else {
                            return Err(VisitorError::GeneralError(String::from("Use Back outside a function")))
                        };
                        let back_type = ret.as_ref().map_or(Ok(Void), |e| self.visit(scope, ret_type, e))?;
                        Scope::set_cur_func_ret_type(scope, back_type);
                    },
                    TokenType::Stop if scope.is_inside_loop() => (),
                    TokenType::Skip if scope.is_inside_loop() => (),
                    TokenType::Skip | TokenType::Stop =>
                         return Err(VisitorError::GeneralError(String::from("Use of Loop Control Flow outside a Loop"))),
                    _ => return Err(VisitorError::NotImplemented(*node.clone())),
                }
                Ok(Void)
            },
            ASTNode::Binary { op: Token { t: tk_type, .. }, l, r } => {
                use TokenType::*;
                let (l_type, r_type) = (self.visit(scope, Unknown, l)?, self.visit(scope, Unknown, r)?);
                // TODO: We can use the known type from of the arms to infer the type of the another it's Unknown
                if !ExprType::expr_type_eq(&l_type, &r_type, true) {
                    return Err(VisitorError::MismatchedTypes(l_type, r_type));
                }
                match *tk_type {
                    Add | Sub | Mul | Div | Shl | Shr | Bor | Band | Bnot | Bxor => { Ok(l_type) },
                    GrE | GrT | LeE | LeT | Neq | Eq | And | Or => { Ok(ExprType::Bool) },
                    _ => panic!("Unknown Binary operator."),
                }
            },
            ASTNode::Unary { op: Token { t: tk_type, .. }, e } => {
                use TokenType::*;
                match tk_type {
                    Bnot | Add => Ok(self.visit(scope, expected_type, e)?),
                    Sub => {
                        let t = self.visit(scope, expected_type, e)?;
                        match t {
                            ExprType::Int { signed: true, .. } | ExprType::Real(_) => Ok(t),
                            _ => panic!("Trying to sign an unsigned integer")
                        }
                    },
                    Not => Ok(self.visit(scope, ExprType::Bool, e)?),
                    _ => panic!("Unknown Binary operator. {:?}", e),
                }
            },
            ASTNode::Leaf(tk) => {
                Ok(match tk.t {
                    TokenType::Character => Char,
                    TokenType::Real => Real(64),
                    TokenType::Integer => Int { bits: 64, signed: true },
                    TokenType::Id => {
                        if let Some(t) = Scope::find_var(scope, &tk.text) { 
                            if t != expected_type {
                                return Err(VisitorError::MismatchedTypes(t, expected_type))
                            } else {
                                let var_type = expected_type.final_type(&t);
                                // Update var type if it is Unknown
                                if t.is_unknown() { Scope::update_var_type(scope, var_type.clone(), &tk.text) }
                                var_type
                            }
                        }
                        else {
                            return Err(VisitorError::VariableNotDeclared(tk.text.clone()))
                        }
                    }
                    _ => return Err(VisitorError::NotImplemented(*node.clone())),
                })
            }
            _ => Err(VisitorError::NotImplemented(*node.clone())),
        }
    }
}
