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

    Struct(Vec<Var>),

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
            Struct(vars) => { write!(f, "Struct ( ")?; vars.fmt(f)?; write!(f, " )") },
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
              (Unknown(i), Unknown(j)) if strict_cmp && i == j => true,

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

    pub fn is_alias(&self) -> bool {
        if let ExprType::Alias(_) = self {
            true
        } else { false }
    }
    fn get_nth_inner_type(&self, nth: usize) -> Self {
        match self {
            FnType(l) | UnionType(l) | TupleType(l) => l[nth].clone(),
            _ => panic!("Cannot get a inner type of a non compound type"),
        }
    }
    fn get_inner_type(&self) -> &Vec<Self> {
        match self {
            FnType(l) | UnionType(l) | TupleType(l) => l,
            _ => panic!("Cannot get the inner type of a non compound type"),
        }
    }
    fn get_fn_return_type(&self) -> Self {
        if let FnType(_) = self {
            self.get_inner_type().last().unwrap().clone()
        } else {
             panic!("Cannot get the function return type of a non function type")
        }
    }
    fn is_unknown(&self) -> bool {
        match self {
            Unknown(_) => true,
            FnType(elems) | UnionType(elems) | TupleType(elems) => elems.iter().any(|e| e.is_unknown()),
            PntVar(inner) | Pnt(inner) => inner.is_unknown(),
            _ => false,
        }
    }
    fn get_inner_from_customtype(&self) -> Self {
        if let CustomType(inner) = self {
            return *inner.clone()
        } else {
            panic!("Trying to get_inner_from_customtype of non CustomType: {self:?}")
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
            ASTNode::Leaf(Token { t: TokenType::True, .. }) => Bool,
            ASTNode::Leaf(Token { t: TokenType::False, .. }) => Bool,
            ASTNode::Leaf(Token { t: TokenType::Real(_), .. }) => Real(64),
            ASTNode::Leaf(Token { t: TokenType::Character(_), .. }) => Char,
            ASTNode::Leaf(Token { t: TokenType::Str(_), .. }) => Pnt(Box::new(Char)),
            ASTNode::Leaf(Token { t: TokenType::Integer(_), .. }) => Int { bits: 64, signed: false },
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

#[derive(Debug, Clone)]
pub struct Var {
    pub is_var: bool,
    pub v: Token,
    pub t: ExprType,
}

#[derive(Debug, Clone)]
pub enum ScopeAttr {
    GlobScope,
    StructScope { name: String },
    FuncScope {
        name: String,
        args_len: usize,
        ret_type: ExprType,
        // parent will only be used when we are creating a function from a partial application of
        // other function, so its parent will be the function which its arguments are missing
        parent: Option<String>,
        is_var: bool,
    },
    CondScope,
    LoopScope {
        label: String,
    }
}

#[derive(Clone)]
pub enum Elem { Var(Var), Scope(Scope), }
impl Elem {
    pub fn get_var(&self) -> &Var {
        if let Elem::Var(v) = self { v }
        else { panic!("Trying to get a variable from a Scope Elem") }
    }

    pub fn get_scp(&self) -> &Scope {
        if let Elem::Scope(s) =  self { s }
        else { panic!("Trying to get a Scope from a Var Elem") }
    }

    pub fn func_as_var(&self) -> Var {
        let scp = self.get_scp();
        scp.func_as_var()
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
        }
    }
}


#[derive(Debug, Clone)]
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

    fn get_cur_func_scp(&mut self) -> Option<&mut Self> {
        if let ScopeAttr::FuncScope { .. } = self.attrs { Some(self) }
        else {
            unsafe { (self.scp_father as *mut Scope).as_mut().map_or(Option::None, |f| f.get_cur_func_scp()) }
        }
    }

    fn func_as_var(&self) -> Var {
        if let Scope { attrs: ScopeAttr::FuncScope { name, args_len, ret_type, is_var, .. }, elems, .. } = self {
            Var {
                is_var: *is_var,
                v: Token::new(0, 0, name),
                t: FnType(
                    if *args_len != 0 {
                        (0..*args_len).map(|i| elems[i].get_var().t.clone()).chain([ret_type.clone()]).collect()
                    } else {
                        vec![None, ret_type.clone()]
                    })
            }
        } else {
            panic!("Passed scope is not a function");
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
    last_def_name: Option<String>,
}

impl Visitor {
    pub fn new(unknown_id: usize) -> Self {
        Visitor { glob_scope: Option::None, cur_scope: std::ptr::null(), unknown_id, unknown_map: vec![Unknown(0); unknown_id+1], last_def_name: Option::None }
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
                    (Elem::Scope(Scope { attrs: ScopeAttr::FuncScope { name, .. }, .. }), "func") if name == elem_name => return Some(e),
                    (Elem::Scope(_), "scope") => {
                        // TODO: A better find for Scopes, maybe search for the ScopeAttr type
                        return Some(e)
                    },
                    (Elem::Var( Var { v: Token { t: TokenType::Id(type_name), .. }, t: CustomType(_), .. }), "type") 
                        if type_name == elem_name => return Some(e),
                    (Elem::Var(Var { v: Token { t: TokenType::Id(var_name), .. }, .. }), "var") if var_name == elem_name => return Some(e),
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

        let (mut f_level, mut s_level) = (0, 0);
        // With infer_type_level we can get the root of each type and the distance to it
        // println!("f: {f:?}, s: {s:?}");
        let (f_infer, s_infer) = (self.infer_type_level(f, &mut f_level), self.infer_type_level(s, &mut s_level));
        // println!("f_infer: {f_infer:?}, s_infer: {s_infer:?}");
        match if f_level > s_level { (f_infer, s_infer) } else { (s_infer, f_infer) }
        {
            (Unknown(big_lvl), Unknown(lit_lvl)) => {
                if big_lvl != lit_lvl {
                    self.unknown_map[lit_lvl] = Unknown(big_lvl)
                }
            },
            (Unknown(i), t) |
            (t, Unknown(i)) => self.unknown_map[i] = t,
            (Alias(n1), Alias(n2)) if n1 == n2 => (),
            (Alias(name), t) |
            (t, Alias(name)) => {
                let elem_type = self.find_elem_type("type", &name).expect("Alias type not defined");
                let alias_type = elem_type.get_var().t.get_inner_from_customtype();
                self.equivalent_types(&t, &alias_type)?;
            },
            (Pnt(inner1), Pnt(inner2)) | (PntVar(inner1), PntVar(inner2)) => {
                self.equivalent_types(&*inner1, &*inner2)?;
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
        // println!("f_end: {:?}, s_end: {:?}", self.infer_type(f), self.infer_type(s));
        Ok(())
    }

    fn infer_type(&self, t: &ExprType) -> ExprType {
        let mut level = 0;
        self.infer_type_level(t, &mut level)
    }

    /// Tries to infer the type by using every known type and equivalences between types
    fn infer_type_level(&self, t: &ExprType, level: &mut usize) -> ExprType {
        *level += 1;
        match t {
            Unknown(ind) => {
                if let Unknown(i) = self.unknown_map[*ind] {
                    if i == 0 { t.clone() }
                    else if i == *ind { panic!("Unknown equals to itself: {t:?}") }
                    else { self.infer_type_level(&Unknown(i), level) }
                }
                else {
                    self.infer_type_level(&self.unknown_map[*ind], level)
                }
            },
            FnType(elems) => FnType(elems.into_iter().map(|e| self.infer_type_level(e, level)).collect()),
            UnionType(elems) => UnionType(elems.into_iter().map(|e| self.infer_type_level(e, level)).collect()),
            TupleType(elems) => TupleType(elems.into_iter().map(|e| self.infer_type_level(e, level)).collect()),
            Alias(name) => self.find_elem_type("type", &name).expect("Alias not defined").get_var().t.get_inner_from_customtype(),
            Pnt(inner) => Pnt(Box::new(self.infer_type_level(&*inner, level))),
            PntVar(inner) => PntVar(Box::new(self.infer_type_level(&*inner, level))),
            _ => t.clone()
        }
    }

    pub fn traverse(&mut self, ast: &mut Vec<Node>) -> Result<(), std::io::Error> {
        use std::io::{Error, ErrorKind};
        let mut global_scope = Scope { attrs: ScopeAttr::GlobScope, elems: vec![], scp_father: std::ptr::null() };
        for n in &mut *ast {
            self.visit(&mut global_scope, None, n).map_err(|e| Error::new(ErrorKind::InvalidInput, format!("Visitor Error: {e:?}")))?;
        }
        self.cur_scope = &global_scope as *const Scope;
        self.glob_scope = Some(global_scope);
        self.update_types(ast)?;
        Ok(())
    }

    fn visit(&mut self, scope: *mut Scope, expected_type: ExprType, node: &mut Node) -> Result<ExprType, VisitorError> {
        let scope = unsafe { &mut *scope };
        self.cur_scope = &*scope as *const Scope;
        match &mut *node.v {
                                                                // TODO: Get different types of TokenType here, ModAccess and StruAccess
            ASTNode::Assign { var: (is_var, ref tk @ Token { t: ref tk_type, .. }, t), expr } => {
                // if the current assignment creates an Function Scope we need to update its name
                // with the current variable name, otherwise we are creating a non-function variable
                let var_name = tk_type.clone().get_id_name();
                self.last_def_name = Some(var_name.to_string());
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
                // If it's Some it means that we still have to create the variable
                // if not, it means that the definition already used this name
                // TODO: Use the 'is_var' with the variables that don't enter this if
                if self.last_def_name.is_some() {
                    if let Some(v) = self.find_elem_type("var", &var_name) {
                        let def = v.get_var().clone();
                        if def.is_var {
                            self.equivalent_types(&def.t, &expression_type)?;
                            return Ok(None)
                        }
                    }
                    scope.elems.push(Elem::Var( Var {
                        is_var: *is_var,
                        v: tk.clone(),
                        t: if let CustomType(_) = expression_type { expression_type } else { self.infer_type(&expected_type)}
                    }));
                }
                Ok(None)
            },
            ASTNode::Struct(items) => {
                let mut inner_types = vec![];
                let mut scp = Scope {
                    attrs: ScopeAttr::StructScope { name: self.last_def_name.take().expect("Struct was not previously defined.") },
                    elems: vec![],
                    scp_father: std::ptr::null(), // Must not access variables from outter scopes by now
                };
                for (i, item) in items.iter_mut().enumerate() {
                    let t = Unknown(self.get_unknown_id());
                    self.visit(&mut scp, t, item)?;
                    inner_types.push(if let Elem::Var(v) = &scp.elems[i] {
                        v.clone()
                    } else {
                        scp.elems[i].func_as_var()
                    });
                }
                self.last_def_name = Option::None;
                scope.elems.push(Elem::Scope(scp));
                Ok(Struct(inner_types))
            },
            ASTNode::Func { args, ret, body } => {
                let ret_type = ret.clone();
                for (_, _, t) in &mut *args {
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
                            .map(|i| args[i].2.as_ref().unwrap().t.clone())
                            .chain([ret_type])
                            .collect()
                    } else {
                        vec![None, ret_type]
                    });

                self.equivalent_types(&fn_type, &expected_type)?;
                let fn_type = self.infer_type(&expected_type);

                let scp = Scope {
                    attrs: ScopeAttr::FuncScope {
                        name: self.last_def_name.take().expect("Function name was not defined previously"),
                        ret_type: fn_type.get_fn_return_type(),
                        args_len: args.len(),
                        parent: Option::None,
                        is_var: false,
                    },
                    elems: (0..args.len()).map(|i| Elem::Var(Var {
                              is_var: args[i].0,
                              v: args[i].1.clone(),
                              t: fn_type.get_nth_inner_type(i),
                          })).collect(),
                    scp_father: scope,
                };
                // Pushing the function scope before its elements are visited to enable recursive functions definitions
                scope.elems.push(Elem::Scope(scp));
                if let Elem::Scope(func_scope_ref) = scope.elems.last_mut().unwrap() {
                    for node in body {
                        self.visit(func_scope_ref, None, node)?;
                    }
                }
                self.last_def_name = Option::None;
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
            ASTNode::FnCall { caller, params, is_sttm } => {
                if let Token { t: TokenType::Id(name), .. } = &caller {

                    // If the FnCall returns a function, partial application, this will be its name
                    // only creates a FuncScope with the parent_func if var_name if Some.
                    let var_name = self.last_def_name.take();
                    let scp_fn = self.find_elem_type("func", name).expect(&format!("Function '{name}' is not defined")).get_scp();
                    let fn_as_var = scp_fn.func_as_var();
                    let fn_type = fn_as_var.t.get_inner_type();
                    let params_types = if let None = fn_type[0] { vec![] } else { fn_type[..fn_type.len()-1].to_vec() };
                    let params_vars: Vec<Elem> = scp_fn.elems[..params_types.len()].iter().map(|e| e.clone()).collect();

                    let cur_ret_type = fn_type.last().unwrap();
                    let ret_type = if *is_sttm { // A function that returns none is a statement
                        self.equivalent_types(cur_ret_type, &None)?;
                        node.t = None;
                        None
                    } else { cur_ret_type.clone() };

                    if params.len() > params_types.len() {
                        panic!("Function receiving more than suported parameters: {node:?}");
                    }

                    let mut i = 0;
                    while i < params.len() {
                        // println!("{:?} :::::: {:?}", params_types[i], params[i] );
                        self.visit(scope, params_types[i].clone(), &mut params[i])?;
                        // println!("here {:?} ::::::: {:?}", params_types[i], self.infer_type(&params_types[i]));
                        i += 1;
                    }
                    let t = if i == params_types.len() {
                        node.t = ret_type.clone();
                        ret_type.clone()
                    }
                    else if let Some(func_name) = var_name {
                        scope.elems.push(Elem::Scope(Scope {
                            attrs: ScopeAttr::FuncScope {
                                name: func_name,
                                args_len: params_types.len()-i,
                                ret_type: ret_type.clone(),
                                parent: Some(name.clone()),
                                is_var: false
                            },
                            elems: params_vars[i..].to_vec(),
                            scp_father: scope.scp_father,
                        }));
                        FnType(fn_type[i..].to_vec())
                    } else {
                        FnType(fn_type[i..].to_vec())
                    };
                    node.t = t.clone();
                    self.equivalent_types(&expected_type, &t)?;
                    let infered_type = self.infer_type(&t);

                    if *is_sttm && !ExprType::expr_type_eq(&infered_type, &None, true) {
                        panic!("Function call of {caller} is a statement but its return is ignored");
                    } else if !*is_sttm && ExprType::expr_type_eq(&infered_type, &None, true) {
                        panic!("Function call of {caller} is an assignment but returns none");
                    }

                    Ok(t)
                } else {
                    panic!("At the moment, the caller can only be the function name")
                }
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
                self.equivalent_types(&expected_type, &node.t)?;
                let expr_type = match *tk_type {
                    Add | Sub | Mul | Div | Shl | Shr | Bor | Band | Bnot | Bxor => { self.infer_type(&l_type) },
                    GrE | GrT | LeE | LeT | Neq | Eq | And | Or => { ExprType::Bool },
                    _ => panic!("Unknown Binary operator."),
                };
                self.equivalent_types(&expected_type, &expr_type)?;
                Ok(self.infer_type(&expected_type))
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
                    TokenType::Character(_) => Char,
                    TokenType::Real(_) => {
                        if let Real(_) = expected_type { expected_type.clone() }
                        else { Real(64) }
                    },
                    TokenType::Integer(_) => {
                        if let Int{ .. } = expected_type { expected_type.clone() }
                        else { Int { bits: 64, signed: false } }
                    },
                    TokenType::Str(_) => Pnt(Box::new(Char)),
                    TokenType::True => Bool,
                    TokenType::False => Bool,
                    TokenType::Id(name) => {
                        if let Some(t) = Scope::find_var(scope, name) {
                            self.equivalent_types(&expected_type, &t)?;
                            self.infer_type(&expected_type)
                        }
                        else {
                            return Err(VisitorError::VariableNotDeclared(name.to_string()))
                        }
                    }
                    _ => return Err(VisitorError::NotImplemented(*node.v.clone())),
                };
                self.equivalent_types(&t, &node.t)?;
                self.equivalent_types(&t, &expected_type)?;
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
            ASTNode::Ref { var, e } => {
                let t = Box::new(match &node.t {
                    Pnt(inner) | PntVar(inner) => self.visit(scope, *inner.clone(), e)?,
                    _ => unreachable!("No other type is expected here"),
                });
                self.equivalent_types(&node.t, &(if *var { PntVar(t) } else { Pnt(t) }))?;
                Ok(node.t.clone())
            },
            ASTNode::Deref { mut n, e, .. } => {
                let deref_t = Unknown(self.get_unknown_id());
                let mut t = self.visit(scope, deref_t, e)?;
                loop {
                    if n == 0 { break }
                    n -= 1;
                    match t {
                        PntVar(inner) | Pnt(inner) => t = *inner,
                        inner @ Unknown(_) => {
                            self.equivalent_types(&expected_type, &inner)?;
                            return Ok(self.infer_type(&expected_type))
                        },
                        _ if n == 1 => (),
                        _ => unreachable!("Tried to deref more than possible at {:?}", e.v),
                    };
                }
                Ok(t)
            },
            ASTNode::Empty => {
                // NOTE: This should only happen when we are creating an alias of some type, a CustomType
                Ok(CustomType(Box::new(node.t.clone())))
            }
            _ => Err(VisitorError::NotImplemented(*node.v.clone()))
        }
    }

    pub fn update_types(&mut self, ast: &mut Vec<Node>) -> Result<(), std::io::Error> {
        for n in ast { self.update_types_aux(n)? }
        Ok(())
    }

    pub fn update_types_aux(&mut self, ast: &mut Node) -> Result<(), std::io::Error> {
        match &mut *ast.v {
            ASTNode::Assign { var: (_, _, Some(node)), expr } => {
                self.update_types_aux(node)?;
                self.update_types_aux(expr)?;
                let final_type = self.infer_type(&node.t);
                node.t = final_type.clone();
                expr.t = final_type;
            },
            ASTNode::Func { args, ret, body } => {
                *ret = self.infer_type(ret);
                for arg in &mut *args {
                    if let (_, _, Some(node)) = arg {
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
            ASTNode::FnCall { params, .. } => self.update_types(params)?,
            ASTNode::Binary { l, r, .. } => {
                self.update_types_aux(l)?;
                self.update_types_aux(r)?;
            },
            ASTNode::Unary { e, .. } => self.update_types_aux(e)?,
            ASTNode::FlowChange(_, e) => if let Some(expr) = e {
                self.update_types_aux(expr)?
            },
            ASTNode::Deref { e, .. } => self.update_types_aux(e)?,
            ASTNode::Ref { e, .. } => self.update_types_aux(e)?,
            ASTNode::Cast { e, .. } => self.update_types_aux(e)?,
            ASTNode::Empty | ASTNode::Leaf(_) => (),
            ASTNode::Struct(vars) => self.update_types(vars)?,
            _ => panic!("Not implemented yet: {ast:?}"),
        }
        if ast.t.is_unknown() {
            ast.t = self.infer_type(&ast.t);
        }
        Ok(())
    }
}
