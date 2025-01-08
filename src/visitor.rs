use crate::{grammar::{Node, InnerNode, ASTNode}, tokenizer::{Token, TokenType}};

use ExprType::*;
#[derive(Clone)]
pub enum ExprType {
    // Types
    Int{ bits: usize, signed: bool }, Real(usize), Char, Bool,

    // Compounded types
    FnType(Vec<ExprType>), UnionType(Vec<ExprType>), TupleType(Vec<ExprType>),

    Type(Box<ExprType>), Void, Unknown(usize)
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
            Type(t) => write!(f, "Type({:?})", **t),
            Void => write!(f, "Void"),
            Unknown(i) => write!(f, "Unknown({i})"),
        }
    }
}
impl ExprType {
    fn expr_type_eq(f: &ExprType, s: &ExprType, strict_cmp: bool) -> bool {
        match (f, s) {
            (Char, Char)
            | (Bool, Bool)
            | (Void, Void) => true,
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
            _ => false,
        }
    }
    /// NOTE: only use this function when you are sure both types are equal,
    /// this function will use every type known from both ExprType to build the
    /// final type, filling every Unknown possible
    fn final_type(self, s: &Self) -> Self {
        let fill_unknown = |l1: Vec<ExprType>, l2: &Vec<ExprType>| {
            l1.into_iter().zip(l2.iter()).map(|(e1, e2)| if !e1.is_unknown() { e1 } else { e2.clone() }).collect()
        };
        if let Unknown(_) = self { return s.clone() }
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
    fn is_unknown(&self) -> bool { if let Unknown(_) = self { true } else { false }}
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
            ASTNode::Type { t: TokenType::Void, inner_types } if inner_types.is_empty() => Void,
            ASTNode::Type { t: TokenType::FnType, inner_types } => FnType(inner_types.iter().map(|it| ExprType::from(it)).collect()),
            ASTNode::Type { t: TokenType::UnionType, inner_types } => UnionType(inner_types.iter().map(|it| ExprType::from(it)).collect()),
            ASTNode::Type { t: TokenType::TupleType, inner_types } => TupleType(inner_types.iter().map(|it| ExprType::from(it)).collect()),
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

pub enum Elem { Var(Var), Scope(Scope) }
impl Elem {
    pub fn get_var(&self) -> &Var {
        if let Elem::Var(v) =  self { v }
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
            }
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
            if scp.is_null() { return None }

            let scp_ref = &*scp;
            for e in scp_ref.elems.iter().rev() {
                if let Elem::Var(var) = e {
                    if var.v.text.as_str() == var_name {
                        return Some(var.t.clone())
                    }
                }
            }
            Scope::find_var(scp_ref.scp_father, var_name)
        }
    }

    fn update_var_type(scp: *mut Scope, t: ExprType, var_name: &str) {
        unsafe {
            if scp.is_null() { return }

            let scp_ref = &mut *scp;
            for e in scp_ref.elems.iter_mut().rev() {
                if let Elem::Var(var) = e {
                    if var.v.text.as_str() == var_name {
                        var.t = t;
                        return
                    }
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
            if let ScopeAttr::FuncScope { ret_type, .. } = &mut scp_ref.attrs {
                if ret_type.is_unknown() || *ret_type == t {
                    *ret_type = t;
                } else {
                    // TODO: make this error better readable, what function occurred?
                    panic!("Returning function with different return types")
                }
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
    pub glob_scope: Option<Scope>,
    pub unknown_map: Vec<ExprType>,
    unknown_id: usize,
}

impl Visitor {
    pub fn new(unknown_id: usize) -> Self {
        Visitor { glob_scope: None, unknown_id, unknown_map: vec![Unknown(0); unknown_id+1] }
    }

    fn get_unknown_id(&mut self) -> usize {
        self.unknown_id += 1;
        self.unknown_map.push(Unknown(0));
        self.unknown_id
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
            }
            (t, t2) => if t != t2 { return Err(VisitorError::MismatchedTypes(f.clone(), s.clone())) }
        };
        Ok(())
    }

    fn get_type(&self, t: ExprType) -> ExprType {
        match t {
            Unknown(ind) => {
                if let Unknown(i) = self.unknown_map[ind] {
                    if i == 0 { t }
                    else { self.get_type(Unknown(i)) }
                }
                else {
                    self.unknown_map[ind].clone()
                }
            },
            FnType(elems) => FnType(elems.into_iter().map(|e| self.get_type(e)).collect()),
            UnionType(elems) => UnionType(elems.into_iter().map(|e| self.get_type(e)).collect()),
            TupleType(elems) => TupleType(elems.into_iter().map(|e| self.get_type(e)).collect()),
            _ => t
        }
    }

    pub fn traverse(&mut self, ast: &mut Vec<Node>) -> Result<(), std::io::Error> {
        use std::io::{Error, ErrorKind};
        let mut global_scope = Scope { attrs: ScopeAttr::GlobScope, elems: vec![], scp_father: std::ptr::null() };
        for n in &mut *ast {
            self.visit(&mut global_scope, Void, n).map_err(|e| Error::new(ErrorKind::InvalidInput, format!("Visitor Error: {e:?}")))?;
        }
        self.glob_scope = Some(global_scope);
        self.update_types(ast)?;
        Ok(())
    }

    fn visit(&mut self, scope: &mut Scope, expected_type: ExprType, node: &mut Node) -> Result<ExprType, VisitorError> {
        match &mut *node.v {
            ASTNode::Assign { var: (tk, t), expr } => {
                // if the current assignment creates an Function Scope we need to update its name
                // with the current variable name, otherwise we are creating a non-function variable
                let assign_index = scope.elems.len();
                if t.is_none() {
                    *t = Some(t.clone()
                               .map_or(
                                   Node::new(Unknown(self.get_unknown_id()),
                                             Box::new(ASTNode::Empty)),
                                   |itself| itself)
                        );
                }
                // Guaranteed it's Some since the if above
                let expected_type = t.as_ref().unwrap().t.clone();
                let expression_type = self.visit(scope, expected_type.clone(), expr)?;
                self.equivalent_types(&expected_type, &expression_type)?;
                if let Some(Elem::Scope(Scope { attrs: ScopeAttr::FuncScope { name, .. }, ..})) = scope.elems.get_mut(assign_index) {
                    *name = String::from(&tk.text);
                } else {
                    scope.elems.push(Elem::Var(Var {
                        v: tk.clone(),
                        t: self.get_type(expected_type),
                    }));
                }
                Ok(Void)
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
                        vec![Void, ret_type]
                    });
                if fn_type == expected_type {
                    self.equivalent_types(&fn_type, &expected_type)?;
                    let fn_type = self.get_type(expected_type);

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
                        // TODO: Void may not be the best type to be expected, but for statements it's fine
                        self.visit(&mut scp, Void, node)?;
                    }
                    scope.elems.push(Elem::Scope(scp));
                    Ok(fn_type)
                } else {
                    Err(VisitorError::MismatchedTypes(fn_type, expected_type))
                }
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
                    self.visit(&mut scp, Void, node)?;
                }
                scope.elems.push(Elem::Scope(scp));
                Ok(Void)
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
                    self.visit(&mut scp, Void, node)?;
                }
                scope.elems.push(Elem::Scope(scp));
                if let Some(n) = next {
                    self.visit(scope, Bool, n)?;
                }
                Ok(Void)
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
                        let back_type = ret.as_mut().map_or(Ok(Void), |e| self.visit(scope, ret_type.clone(), e))?;
                        self.equivalent_types(&back_type, &ret_type)?;
                    },
                    TokenType::Stop if scope.is_inside_loop() => (),
                    TokenType::Skip if scope.is_inside_loop() => (),
                    TokenType::Skip | TokenType::Stop =>
                         return Err(VisitorError::GeneralError(String::from("Use of Loop Control Flow outside a Loop"))),
                    _ => return Err(VisitorError::NotImplemented(*node.v.clone())),
                }
                Ok(Void)
            },
            ASTNode::Binary { op: Token { t: tk_type, .. }, l, r } => {
                use TokenType::*;
                let branch_type = Unknown(self.get_unknown_id());
                let (l_type, r_type) = (self.visit(scope, branch_type.clone(), l)?, self.visit(scope, branch_type, r)?);
                self.equivalent_types(&l_type, &r_type)?;
                self.equivalent_types(&r_type, &node.t)?;
                let expr_type = match *tk_type {
                    Add | Sub | Mul | Div | Shl | Shr | Bor | Band | Bnot | Bxor => { self.get_type(l_type) },
                    GrE | GrT | LeE | LeT | Neq | Eq | And | Or => { ExprType::Bool },
                    _ => panic!("Unknown Binary operator."),
                };
                self.equivalent_types(&expected_type, &expr_type)?;
                Ok(self.get_type(expected_type))
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
                let t = match tk.t {
                    TokenType::Character => Char,
                    TokenType::Real => Real(64),
                    TokenType::Integer => Int { bits: 64, signed: true },
                    TokenType::Id => {
                        if let Some(t) = Scope::find_var(scope, &tk.text) {
                            if t != expected_type {
                                return Err(VisitorError::MismatchedTypes(t, expected_type))
                            } else {
                                self.equivalent_types(&expected_type, &t)?;
                                self.get_type(expected_type)
                            }
                        }
                        else {
                            return Err(VisitorError::VariableNotDeclared(tk.text.clone()))
                        }
                    }
                    _ => return Err(VisitorError::NotImplemented(*node.v.clone())),
                };
                self.equivalent_types(&t, &node.t)?;
                Ok(t)
            },
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
                let final_type = self.get_type(node.t.clone());
                node.t = final_type.clone();
                expr.t = final_type;
            },
            ASTNode::Func { args, ret: Some(Node { t, .. }), body } => {
                *t = self.get_type(t.clone());
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
            _ => (),
        }
        if let t @ Unknown(_) = &mut ast.t {
            *t = self.get_type(t.clone());
        }
        Ok(())
    }
}
