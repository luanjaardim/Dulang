use crate::{grammar::{Node, InnerNode, ASTNode}, tokenizer::{Token, TokenType}};

use ExprType::*;
#[derive(Clone)]
pub enum ExprType {
    // Types
    Int{ bits: usize, signed: bool }, Real(usize), Char, Bool,

    // Compounded types
    FnType(Vec<ExprType>), UnionType(Vec<ExprType>), TupleType(Vec<ExprType>),

    // Pointer types
    Pnt(Box<ExprType>), PntVar(Box<ExprType>),

    Type, CustomType(Box<ExprType>), Alias(String), None, Unknown(usize)
}
impl std::fmt::Debug for ExprType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Int { bits, signed } => write!(f, "Int({bits}, is_signed: {signed})"),
            Real(bits) => write!(f, "Real({bits})"),
            Char => write!(f, "Char"),
            Bool => write!(f, "Bool"),
            FnType(e) => {
                write!(f, "(Fn: ")?;
                for i in 0..e.len() { e[i].fmt(f)?; if i < e.len()-1 { write!(f, ", ")? } }
                write!(f, ")")
            },
            UnionType(e) => {
                write!(f, "(Union: ")?;
                for i in 0..e.len() { e[i].fmt(f)?; if i < e.len()-1 { write!(f, ", ")? } }
                write!(f, ")")
            },
            TupleType(e) => {
                write!(f, "(Tuple: ")?;
                for i in 0..e.len() { e[i].fmt(f)?; if i < e.len()-1 { write!(f, ", ")? } }
                write!(f, ")")
            },
            CustomType(t) => { write!(f, "CustomType( ")?; t.fmt(f)?; write!(f, " )") }
            Alias(s) => write!(f, "Alias({s})"),
            Pnt(t) => { write!(f, "Pnt to ( ")?; t.fmt(f)?; write!(f, " )") },
            PntVar(t) => { write!(f, "PntVar to ( ")?; t.fmt(f)?; write!(f, " )") },
            Type => write!(f, "Type"),
            None => write!(f, "None"),
            Unknown(i) => write!(f, "Unknown({i})"),
        }
    }
}
impl ExprType {
    fn expr_type_eq(f: &ExprType, s: &ExprType, strict_cmp: bool) -> bool {
        match (f, s) {
            (Char, Char)
            | (Bool, Bool)
            | (None, None)
            | (Type, Type) => true,
              (_, Unknown(_)) if !strict_cmp => true,
              (Unknown(_), _) if !strict_cmp => true,

            (Real(b1), Real(b2)) if b1 == b2 => true,
            (Int { bits: b1, signed: s1 }, Int { bits: b2, signed: s2 }) if b1 == b2 && s1 == s2 => true,

            (FnType(l1), FnType(l2))
            | (UnionType(l1), UnionType(l2))
            | (TupleType(l1), TupleType(l2)) => {
               if l1.len() != l2.len() { return false }
               l1.iter().enumerate().all(|(i, e)| Self::expr_type_eq(e, &l2[i], strict_cmp))
            },

            (Pnt(t1), Pnt(t2)) |
            (PntVar(t1), PntVar(t2)) => Self::expr_type_eq(&**t1, &**t2, strict_cmp),
            _ => false,
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

impl From<&InnerNode> for ExprType {
    fn from(value: &InnerNode) -> Self {
        match &**value {
            ASTNode::Type { t: TokenType::I(bits), inner_types } if inner_types.is_empty() => Int { bits: *bits, signed: true },
            ASTNode::Type { t: TokenType::U(bits), inner_types } if inner_types.is_empty() => Int { bits: *bits, signed: false },
            ASTNode::Type { t: TokenType::F(bits), inner_types } if inner_types.is_empty() => Real(*bits),
            ASTNode::Type { t: TokenType::Bool, inner_types } if inner_types.is_empty() => Bool,
            ASTNode::Type { t: TokenType::Char, inner_types } if inner_types.is_empty() => Char,
            ASTNode::Type { t: TokenType::None, inner_types } if inner_types.is_empty() => None,
            ASTNode::Type { t: TokenType::Type, inner_types } if inner_types.is_empty() => Type,
            ASTNode::Type { t: TokenType::Ref, inner_types } if inner_types.len() == 1 => Pnt(Box::new(ExprType::from(&inner_types[0]))),
            ASTNode::Type { t: TokenType::VarRef, inner_types } if inner_types.len() == 1 => PntVar(Box::new(ExprType::from(&inner_types[0]))),
            ASTNode::Type { t: TokenType::FnType, inner_types } => FnType(inner_types.iter().map(|it| ExprType::from(it)).collect()),
            ASTNode::Type { t: TokenType::UnionType, inner_types } => UnionType(inner_types.iter().map(|it| ExprType::from(it)).collect()),
            ASTNode::Type { t: TokenType::TupleType, inner_types } => TupleType(inner_types.iter().map(|it| ExprType::from(it)).collect()),
            ASTNode::Type { t: TokenType::Id(name), inner_types } if inner_types.is_empty() => Alias(name.clone()),
            _ => panic!("Unknown conversion between ASTNode and ExprType"),
        }
    }
}

#[derive(Debug)]
pub struct Var {
    pub v: Token,
    pub t: ExprType,
}

#[derive(Debug)]
pub enum ScopeAttr {
    GlobScope,
    FuncScope {
        name: String,
        args_len: usize,
        ret_type: ExprType,
    },
    CondScope,
    LoopScope {
        label: String,
    }
}

pub enum Elem { Var(Var), Scope(Scope), Type(String, ExprType) }
impl Elem {
    pub fn get_var(&self) -> &Var {
        if let Elem::Var(v) =  self { v }
        else { panic!("Trying to get a variable from a Scope Elem") }
    }

    pub fn get_type(&self) -> &ExprType {
        if let Elem::Type(s, t) =  self { t }
        else { panic!("Trying to get a variable from a Scope Elem") }
    }

    pub fn get_scp(&self) -> &Scope {
        if let Elem::Scope(s) =  self { s }
        else { panic!("Trying to get a Scope from a Var Elem") }
    }
}
impl std::fmt::Debug for Elem {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Elem::Var(v) => {
                write!(f, "{v:#?}")
            },
            Elem::Scope(s) => {
                write!(f, "{s:#?}")
            },
            Elem::Type(s, t) => {
                write!(f, "Alias: {s} -> {t:#?}")
            },
        }
    }
}


#[derive(Debug)]
pub struct Scope {
    pub attrs: ScopeAttr,
    pub elems: Vec<Elem>,
    scp_father: *const Scope,
}

impl Scope {
    fn find_var(scp: *const Scope, var_name: &str) -> Option<ExprType> {
        unsafe {
            if scp.is_null() { return Option::None }

            let scp_ref = &*scp;
            for e in scp_ref.elems.iter().rev() {
                if let Elem::Var(var) = e {
                    if let Token { t: TokenType::Id(name), .. } = &var.v {
                        if name == var_name {
                            return Some(var.t.clone())
                        }
                    } else { unreachable!("Token type should be an Id") }
                }
            }
            Scope::find_var(scp_ref.scp_father, var_name)
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
            unsafe { self.scp_father.as_ref().map_or(Option::None, |f| f.get_cur_func_scp()) }
        }
    }

}

#[derive(Debug)]
pub enum VisitorError {
    VariableNotDeclared(String),
    MismatchedTypes(ExprType, ExprType),
    CastError(ExprType, ExprType),
    GeneralError(String),
    NotImplemented(ASTNode)
}

pub struct Visitor {
    pub glob_scope: Option<Scope>,
    cur_scope: *const Scope,
    pub unknown_map: Vec<ExprType>,
    unknown_id: usize,
}

impl Visitor {
    pub fn new(unknown_id: usize) -> Self {
        Visitor { glob_scope: Option::None, cur_scope: std::ptr::null(), unknown_id, unknown_map: vec![Unknown(0); unknown_id+1] }
    }

    fn get_unknown_id(&mut self) -> usize {
        self.unknown_id += 1;
        self.unknown_map.push(Unknown(0));
        self.unknown_id
    }

    fn find_elem_type(&self, t: &str, elem_name: &str) -> Option<&Elem> {
        let mut scp_ref = unsafe {
             &*self.cur_scope
        };
        loop {
            for e in scp_ref.elems.iter().rev() {
                match (e, t) {
                    (Elem::Scope(_), "scope") => {
                        // TODO: A better find for Scopes, maybe search for the ScopeAttr type
                        return Some(e)
                    },
                    (Elem::Var(Var { v: Token { t: TokenType::Id(var_name), .. }, t }), "var") => {
                        if var_name == elem_name {
                            return Some(e)
                        }
                    },
                    (Elem::Type(type_name, ..), "type") => {
                        if type_name == elem_name {
                            return Some(e)
                        }
                    },
                    _ => ()
                }
            }
            scp_ref = unsafe {
                if scp_ref.scp_father.is_null() {
                    break
                } else {
                    &*scp_ref.scp_father
                }
            };
        }
        Option::None
    }

    fn equivalent_types(&mut self, f: &ExprType, s: &ExprType) -> Result<(), VisitorError> {
        match (f, s) {
            (Unknown(f_ind), Unknown(s_ind)) => {
                let (first, second) = (self.unknown_map[*f_ind].clone(), self.unknown_map[*s_ind].clone());
                if let (Unknown(ind1), Unknown(ind2)) = (&first, &second) {
                    if *ind1 != 0 {
                        self.unknown_map[*s_ind] = Unknown(*ind1);
                    } else if *ind2 != 0 {
                        self.unknown_map[*f_ind] = Unknown(*ind2);
                    } else if *ind1 == 0 && *ind2 == 0 {
                        self.unknown_map[*s_ind] = Unknown(*f_ind);
                    } else {
                        self.equivalent_types(&first, &second)?;
                    }
                } else {
                    panic!("shit bro....");
                }
            },
            (Unknown(i), t) |
            (t, Unknown(i)) => {
                self.unknown_map[*i] = t.clone();
            },
            (Alias(n1), Alias(n2)) if n1 == n2 => (),
            (Alias(name), t) |
            (t, Alias(name)) => {
                let elem_type = self.find_elem_type("type", name).expect("Alias type not defined");
                let alias_type = elem_type.get_type().clone();
                self.equivalent_types(t, &alias_type)?;
            },
            (FnType(inner1), FnType(inner2)) |
            (UnionType(inner1), UnionType(inner2)) |
            (TupleType(inner1), TupleType(inner2)) => {
                if inner1.len() != inner2.len() {
                    return Err(VisitorError::MismatchedTypes(f.clone(), s.clone()))
                } else {
                    for (e1, e2) in inner1.iter().zip(inner2.iter()) {
                        self.equivalent_types(e1, e2)?;
                    }
                }
            },
            (t, t2) => if t != t2 { return Err(VisitorError::MismatchedTypes(f.clone(), s.clone())) }
        };
        Ok(())
    }

    /// Tries to infer the type by using every known type and equivalences between types
    fn infer_type(&self, t: ExprType) -> ExprType {
        match t {
            Unknown(ind) => {
                if let Unknown(i) = self.unknown_map[ind] {
                    if i == 0 { t }
                    else { self.infer_type(Unknown(i)) }
                }
                else {
                    self.unknown_map[ind].clone()
                }
            },
            FnType(elems) => FnType(elems.into_iter().map(|e| self.infer_type(e)).collect()),
            UnionType(elems) => UnionType(elems.into_iter().map(|e| self.infer_type(e)).collect()),
            TupleType(elems) => TupleType(elems.into_iter().map(|e| self.infer_type(e)).collect()),
            Alias(name) => self.find_elem_type("type", &name).expect("Alias not defined").get_type().clone(),
            _ => t
        }
    }

    pub fn traverse(&mut self, ast: &mut Vec<Node>) -> Result<(), std::io::Error> {
        use std::io::{Error, ErrorKind};
        let mut global_scope = Scope { attrs: ScopeAttr::GlobScope, elems: vec![], scp_father: std::ptr::null() };
        for n in &mut *ast {
            self.visit(&mut global_scope, None, n).map_err(|e| Error::new(ErrorKind::InvalidInput, format!("Visitor Error: {e:?}")))?;
        }
        self.glob_scope = Some(global_scope);
        self.update_types(ast)?;
        Ok(())
    }

    fn visit(&mut self, scope: &mut Scope, expected_type: ExprType, node: &mut Node) -> Result<ExprType, VisitorError> {
        self.cur_scope = &*scope as *const Scope;
        match &mut *node.v {
            ASTNode::Assign { var: (ref tk @ Token { t: TokenType::Id(ref var_name), .. }, t), expr } => {
                // if the current assignment creates an Function Scope we need to update its name
                // with the current variable name, otherwise we are creating a non-function variable
                let assign_index = scope.elems.len();
                *t = Some(t.clone()
                           .map_or(
                               Node::new(Unknown(self.get_unknown_id()),
                                         Box::new(ASTNode::Empty)),
                               |itself| itself)
                    );
                // Guaranteed it's Some since the if above
                let expected_type = t.as_ref().unwrap().t.clone();
                let expression_type = self.visit(scope, expected_type.clone(), expr)?;
                self.equivalent_types(&expected_type, &expression_type)?;
                if let Some(Elem::Scope(Scope { attrs: ScopeAttr::FuncScope { name, .. }, ..})) = scope.elems.get_mut(assign_index) {
                    *name = String::from(var_name.clone());
                } else {
                    scope.elems.push(if let CustomType(alias) = expression_type {
                        Elem::Type(var_name.clone(), *alias.clone())
                    } else {
                        Elem::Var(Var {
                            v: tk.clone(),
                            t: self.infer_type(expected_type),
                        })
                    });
                }
                Ok(None)
            },
            ASTNode::Func { args, ret, body } => {
                *ret = Some(ret.clone().map_or(
                                       Node::new(Unknown(self.get_unknown_id()),
                                                 Box::new(ASTNode::Empty)),
                                       |itself| itself));
                let ret_type = ret.as_ref().unwrap().t.clone();
                for (_, t) in &mut *args {
                    if t.is_none() {
                        *t = Some(t.clone()
                                   .map_or(
                                       Node::new(Unknown(self.get_unknown_id()),
                                                 Box::new(ASTNode::Empty)),
                                       |itself| itself)
                            );
                    }
                }
                let fn_type = FnType(
                    if !args.is_empty() {
                        (0..args.len())
                            .map(|i| args[i].1.as_ref().unwrap().t.clone())
                            .chain([ret_type])
                            .collect()
                    } else {
                        vec![None, ret_type]
                    });

                self.equivalent_types(&fn_type, &expected_type)?;
                let fn_type = self.infer_type(expected_type);

                let mut scp = Scope {
                    attrs: ScopeAttr::FuncScope {
                        name: String::new(), // will be filled when return the function call
                        ret_type: fn_type.get_fn_return_type(),
                        args_len: args.len(),
                    },
                    elems: (0..args.len()).map(|i| Elem::Var(Var {
                              v: args[i].0.clone(),
                              t: fn_type.get_nth_inner_type(i),
                          })).collect(),
                    scp_father: scope,
                };
                for node in body {
                    self.visit(&mut scp, None, node)?;
                }
                scope.elems.push(Elem::Scope(scp));
                Ok(fn_type)
            },
            ASTNode::Loop { cond, body } => {
                if let Some(cond_expr) = cond {
                    self.visit(scope, Bool, cond_expr)?;
                }
                let mut scp = Scope {
                    // TODO: implement label declaration for loops
                    attrs: ScopeAttr::LoopScope { label: String::new() },
                    elems: vec![],
                    scp_father: scope,
                };
                for node in body {
                    self.visit(&mut scp, None, node)?;
                }
                scope.elems.push(Elem::Scope(scp));
                Ok(None)
            },
            ASTNode::Conditional { cond, body, next } => {
                if let Some(cond_expr) = cond {
                    self.visit(scope, Bool, cond_expr)?;
                }
                let mut scp = Scope {
                    attrs: ScopeAttr::CondScope,
                    elems: vec![],
                    scp_father: scope,
                };
                for node in body {
                    self.visit(&mut scp, None, node)?;
                }
                scope.elems.push(Elem::Scope(scp));
                if let Some(n) = next {
                    self.visit(scope, Bool, n)?;
                }
                Ok(None)
            },
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
                        let back_type = ret.as_mut().map_or(Ok(None), |e| self.visit(scope, ret_type.clone(), e))?;
                        self.equivalent_types(&back_type, &ret_type)?;
                    },
                    TokenType::Stop if scope.is_inside_loop() => (),
                    TokenType::Skip if scope.is_inside_loop() => (),
                    TokenType::Skip | TokenType::Stop =>
                         return Err(VisitorError::GeneralError(String::from("Use of Loop Control Flow outside a Loop"))),
                    _ => return Err(VisitorError::NotImplemented(*node.v.clone())),
                }
                Ok(None)
            },
            ASTNode::Binary { op: Token { t: tk_type, .. }, l, r } => {
                use TokenType::*;
                let branch_type = Unknown(self.get_unknown_id());
                let (l_type, r_type) = (self.visit(scope, branch_type.clone(), l)?, self.visit(scope, branch_type, r)?);
                self.equivalent_types(&l_type, &r_type)?;
                self.equivalent_types(&r_type, &node.t)?;
                let expr_type = match *tk_type {
                    Add | Sub | Mul | Div | Shl | Shr | Bor | Band | Bnot | Bxor => { self.infer_type(l_type) },
                    GrE | GrT | LeE | LeT | Neq | Eq | And | Or => { ExprType::Bool },
                    _ => panic!("Unknown Binary operator."),
                };
                self.equivalent_types(&expected_type, &expr_type)?;
                Ok(self.infer_type(expected_type))
            },
            ASTNode::Unary { op: Token { t: tk_type, .. }, e } => {
                use TokenType::*;
                let t = match tk_type {
                    Bnot | Add => self.visit(scope, expected_type, e)?,
                    Sub => {
                        let t = self.visit(scope, expected_type, e)?;
                        match t {
                            ExprType::Int { signed: true, .. } | ExprType::Real(_) => t,
                            _ => panic!("Trying to sign an unsigned integer")
                        }
                    },
                    Not => self.visit(scope, ExprType::Bool, e)?,
                    _ => panic!("Unknown Binary operator. {:?}", e),
                };
                self.equivalent_types(&t, &node.t)?;
                Ok(t)
            },
            ASTNode::Leaf(tk) => {
                let t = match &tk.t {
                    TokenType::Character => Char,
                    TokenType::Real => Real(64),
                    TokenType::Integer => Int { bits: 64, signed: true },
                    TokenType::Id(name) => {
                        if let Some(t) = Scope::find_var(scope, name) {
                            self.equivalent_types(&expected_type, &t)?;
                            self.infer_type(expected_type)
                        }
                        else {
                            return Err(VisitorError::VariableNotDeclared(name.to_string()))
                        }
                    }
                    _ => return Err(VisitorError::NotImplemented(*node.v.clone())),
                };
                self.equivalent_types(&t, &node.t)?;
                Ok(t)
            },
            ASTNode::Cast { e, t } => {
                let e_type = self.visit(scope, expected_type, e)?;
                Ok(match (&e_type, &*t) {
                    (None, None) => None,
                    (FnType(_), FnType(_)) | // At the moment it's not possible cast any function type
                    (None, _) |
                    (_, None) => {
                        return Err(VisitorError::CastError(e_type.clone(), t.clone()))
                    },
                    _ => {
                        node.t = t.clone();
                        t.clone()
                    }
                })
            },
            ASTNode::Empty => {
                // NOTE: This should only happen when we are creating an alias of some type, a CustomType
                Ok(CustomType(Box::new(node.t.clone())))
            }
            _ => Err(VisitorError::NotImplemented(*node.v.clone()))
        }
    }

    pub fn update_types(&mut self, ast: &mut Vec<Node>) -> Result<(), std::io::Error> {
        use std::io::{Error, ErrorKind};
        for n in ast {
            self.update_types_aux(n)?;
        }
        Ok(())
    }

    pub fn update_types_aux(&mut self, ast: &mut Node) -> Result<(), std::io::Error> {
        match &mut *ast.v {
            ASTNode::Assign { var: (_, Some(node)), expr } => {
                self.update_types_aux(node)?;
                self.update_types_aux(expr)?;
                let final_type = self.infer_type(node.t.clone());
                node.t = final_type.clone();
                expr.t = final_type;
            },
            ASTNode::Func { args, ret: Some(Node { t, .. }), body } => {
                *t = self.infer_type(t.clone());
                for arg in &mut *args {
                    if let (_, Some(node)) = arg {
                        self.update_types_aux(node)?;
                    }
                }
                self.update_types(body)?;
            },
            ASTNode::Loop { cond, body } => {
                if let Some(condition) = cond {
                    self.update_types_aux(condition)?;
                }
                self.update_types(body)?;
            }
            ASTNode::Conditional { cond, body, next } => {
                if let Some(condition) = cond {
                    self.update_types_aux(condition)?;
                }
                self.update_types(body)?;
                if let Some(node) = next {
                    self.update_types_aux(node)?;
                }
            },
            ASTNode::Binary { l, r, .. } => {
                self.update_types_aux(l)?;
                self.update_types_aux(r)?;
            },
            ASTNode::Unary { e, .. } => self.update_types_aux(e)?,
            ASTNode::FlowChange(_, e) => if let Some(expr) = e {
                self.update_types_aux(expr)?
            },
            ASTNode::Cast { e, .. } => self.update_types_aux(e)?,
            _ => (),
        }
        if let t @ Unknown(_) = &mut ast.t {
            *t = self.infer_type(t.clone());
        }
        Ok(())
    }
}
