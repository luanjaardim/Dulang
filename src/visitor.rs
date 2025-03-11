use std::collections::BTreeSet;
use crate::{grammar::{Node, InnerNode, ASTNode}, tokenizer::{Token, TokenType}};


use ExprType::*;
#[derive(Clone, Ord, PartialOrd, Eq)]
pub enum ExprType {
    // Types
    Int{ bits: usize, signed: bool }, Real(usize), Char, Bool,

    // Compounded types
    FnType(Vec<ExprType>), UnionType(BTreeSet<ExprType>), TupleType(Vec<ExprType>),

    // Pointer types
    Pnt(Box<ExprType>), PntVar(Box<ExprType>),

    Struct(Vec<Var>), StructInstance(String), Array(Box<ExprType>, usize),

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
                let (len, mut i) = (e.len(), 0);
                for elem in e {
                    elem.fmt(f)?;
                    if i < len-1 { write!(f, ", ")? }
                    i += 1
                }
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
            Array(elems_type, len) => { write!(f, "Array [ ")?; elems_type.fmt(f)?; write!(f, "; {len} ]") },
            Type => write!(f, "Type"),
            None => write!(f, "None"),
            Struct(vars) => { write!(f, "Struct ( ")?; vars.fmt(f)?; write!(f, " )") },
            StructInstance(s) => write!(f, "StructInstance({s})"),
            Unknown(i) => write!(f, "Unknown({i})"),
        }
    }
}
impl ExprType {
    pub fn expr_type_eq(f: &ExprType, s: &ExprType, strict_cmp: bool) -> bool {
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
            | (TupleType(l1), TupleType(l2)) => {
               if l1.len() != l2.len() { return false }
               l1.iter().enumerate().all(|(i, e)| Self::expr_type_eq(e, &l2[i], strict_cmp))
            },
            (UnionType(s1), UnionType(s2)) => s1 == s2,
            (StructInstance(name), StructInstance(name2)) if name == name2 => true,

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
            FnType(l) | TupleType(l) => l[nth].clone(),
            _ => panic!("Cannot get a inner type of a non compound type"),
        }
    }
    pub fn get_inner_type(&self) -> &Vec<Self> {
        match self {
            FnType(l) | TupleType(l) => l,
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
            FnType(elems) | TupleType(elems) => elems.iter().any(|e| e.is_unknown()),
            UnionType(elems) => elems.iter().any(|e| e.is_unknown()),
            PntVar(inner) | Pnt(inner) => inner.is_unknown(),
            _ => false,
        }
    }
    fn get_inner_if_customtype(self) -> Self {
        if let CustomType(inner) = self {
            return *inner
        } else {
            self
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
            ASTNode::Type { t: TokenType::Arr(len), inner_types } if inner_types.len() == 1 => Array(Box::new(ExprType::from(&inner_types[0])), *len),
            ASTNode::Type { t: TokenType::FnType, inner_types } => FnType(inner_types.iter().map(|it| ExprType::from(it)).collect()),
            ASTNode::Type { t: TokenType::UnionType, inner_types } => UnionType(inner_types.iter().map(|it| ExprType::from(it)).collect()),
            ASTNode::Type { t: TokenType::TupleType, inner_types } => TupleType(inner_types.iter().map(|it| ExprType::from(it)).collect()),
            ASTNode::Type { t: TokenType::Id(name), inner_types } if inner_types.is_empty() => Alias(name.clone()),
            _ => panic!("Unknown conversion between ASTNode and ExprType"),
        }
    }
}

#[derive(Debug, Clone, Eq, PartialEq, Ord, PartialOrd)]
pub struct Var {
    pub is_var: bool,
    pub v: Token,
    pub t: ExprType,
}

#[derive(Debug, Clone)]
pub enum ScopeAttr {
    GlobScope,
    StructScope { name: String },
    TupleScope { name: String, t: ExprType },
    ModScope { name: String },
    FuncScope {
        name: String,
        args_len: usize,
        ret_type: ExprType,
        // parent will only be used when we are creating a function from a partial application of
        // other function, so its parent will be the function which its arguments are missing
        parent: Option<String>,
        captured_vars: Vec<Var>,
        is_var: bool,
    },
    CondScope,
    LoopScope {
        label: String,
    }
}

#[derive(Clone)]
pub enum Elem { Var(Var), Scope(Scope), Captured(Box<Elem>), }
impl Elem {
    pub fn get_name(&self) -> Option<&str> {
        match self {
            Elem::Var(v) => Some(v.v.t.get_id_name().unwrap()),
            Elem::Scope(scp) => scp.get_name(),
            Elem::Captured(e) => e.get_name(),
        }
    }

    pub fn get_var(&self) -> &Var {
        if let Elem::Var(v) = self { v }
        else { panic!("Trying to get a variable from a Scope Elem") }
    }

    pub fn get_scp(&self) -> &Scope {
        if let Elem::Scope(s) =  self { s }
        else { panic!("Trying to get a Scope from a Var Elem") }
    }

    pub fn get_type(&self) -> ExprType {
        match self {
            Elem::Var(v) => v.t.clone(),
            Elem::Scope(Scope { attrs: ScopeAttr::StructScope { name }, .. }) => StructInstance(name.to_string()),
            _ => panic!("Trying to get a type from a non-type Elem")
        }
    }

    pub fn scp_as_var(&self) -> Var {
        let scp = self.get_scp();
        scp.scp_as_var()
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
            Elem::Captured(e) => write!(f, "(Captured: {})", e.get_name().unwrap()),
        }
    }
}


#[derive(Debug, Clone)]
pub struct Scope {
    pub attrs: ScopeAttr,
    pub elems: Vec<Elem>,
    pub scp_father: *const Scope,
}

impl Scope {

    fn is_inside_loop(&self) -> bool {
        if let ScopeAttr::LoopScope { .. } = self.attrs { true }
        else {
            unsafe { self.scp_father.as_ref().map_or(false, |f| f.is_inside_loop()) }
        }
    }

    pub fn get_name(&self) -> Option<&str> {
        match self {
            Scope { attrs: ScopeAttr::ModScope { name }, ..}    |
            Scope { attrs: ScopeAttr::StructScope { name }, ..} |
            Scope { attrs: ScopeAttr::TupleScope { name, .. }, ..} |
            Scope { attrs: ScopeAttr::FuncScope { name, .. }, ..} => Some(name),
            _ => Option::None,
        }
    }

    pub fn get_cur_func_scp(&self) -> Option<&Self> {
        if let ScopeAttr::FuncScope { .. } = self.attrs { Some(self) }
        else {
            unsafe { (self.scp_father as *mut Scope).as_mut().map_or(Option::None, |f| f.get_cur_func_scp()) }
        }
    }

    pub fn scp_as_var(&self) -> Var {
        match self {
            Scope { attrs: ScopeAttr::FuncScope { name, args_len, ret_type, is_var, .. }, elems, .. } => {
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
            },
            Scope { attrs: ScopeAttr::TupleScope { name, t }, .. } => {
                Var {
                    is_var: false,
                    v: Token::new(0, 0, name),
                    t: t.clone(),
                }
            },
            _ => panic!("Passed scope cannot be reduced to a variable"),
        }
    }

    pub fn find_elem_type(
        &self,
        t: &str,
        elem_name: &str,
        mut start_ind: Option<isize>,
        should_search_parent: bool,
        ignore_captured: bool
    ) -> Option<&Elem> {
        let mut scp_ref = self;
        let mut last_pos = 0;

        'inf_loop :loop {
            let (cur_t, cur_elem_name) = if let Some(pos) = (&elem_name[last_pos..]).find(&[':', '.']) {
                let cur_elem_name = &elem_name[last_pos..last_pos+pos];
                last_pos += pos + 1;
                (if &elem_name[last_pos-1..last_pos] == "." {"field"} else {"mod"}, cur_elem_name)
            } else {
                (t, &elem_name[last_pos..])
            };
            if !scp_ref.elems.is_empty() {
                let end = start_ind.take().unwrap_or(isize::MAX).min(scp_ref.elems.len() as isize -1);
                if end >= 0 {
                    for (i, e) in scp_ref.elems[..=end as usize].iter().enumerate().rev() {
                        match (e, cur_t) {
                            (Elem::Captured(e), _) if !ignore_captured && e.get_name().unwrap() == cur_elem_name => {
                                println!("Variable {:?} was captured and is no longer available", e.get_name().unwrap());
                                return Option::None
                            },
                            (Elem::Var( Var { v: Token { t: TokenType::Nl, .. }, t: StructInstance(struct_name), .. }), _) => {
                                if let Some(parent_struct) = scp_ref.find_elem_type("struct", struct_name, Some(i as isize - 1), true, ignore_captured) {
                                    let tmp_scp = parent_struct.get_scp();
                                    if let ret @ Some(_) = tmp_scp.find_elem_type("any", cur_elem_name, Option::None, should_search_parent, ignore_captured) {
                                        return ret
                                    }
                                    // If it's not an element of the parent struct, continue the search
                                } else {
                                    panic!("Parent struct {struct_name} does not exist.")
                                }
                            },
                            (Elem::Scope(Scope { attrs: ScopeAttr::TupleScope { name, .. }, .. }), "any") |
                            (Elem::Scope(Scope { attrs: ScopeAttr::StructScope { name }, .. }), "any") |
                            (Elem::Scope(Scope { attrs: ScopeAttr::FuncScope { name, .. }, .. }), "any") |
                            (Elem::Scope(Scope { attrs: ScopeAttr::StructScope { name }, .. }), "struct") |
                            (Elem::Scope(Scope { attrs: ScopeAttr::FuncScope { name, .. }, .. }), "func") if name == cur_elem_name => return Some(e),

                            (Elem::Var(Var { t: StructInstance(name), .. }), "field") => {
                                scp_ref = scp_ref.find_elem_type("struct", name, Option::None, should_search_parent, ignore_captured).unwrap().get_scp();
                                continue 'inf_loop;
                            },
                            (Elem::Scope(scope @ Scope { attrs: ScopeAttr::StructScope { name }, .. }), "field") |
                            (Elem::Scope(scope @ Scope { attrs: ScopeAttr::TupleScope { name, .. }, .. }), "field") |
                            (Elem::Scope(scope @ Scope { attrs: ScopeAttr::ModScope { name }, .. }), "mod") if name == cur_elem_name => {
                                scp_ref = scope;
                                continue 'inf_loop;
                            },
                            (Elem::Scope(_), "scope") => {
                                // TODO: A better find for Scopes, maybe search for the ScopeAttr type
                                return Some(e)
                            },
                            (Elem::Scope(Scope { attrs: ScopeAttr::StructScope { name: type_name }, .. }), "any") |
                            (Elem::Var( Var { v: Token { t: TokenType::Id(type_name), .. }, t: CustomType(_), .. }), "any") |
                            (Elem::Scope(Scope { attrs: ScopeAttr::StructScope { name: type_name }, .. }), "type") |
                            (Elem::Var( Var { v: Token { t: TokenType::Id(type_name), .. }, t: CustomType(_), .. }), "type") 
                                if type_name == cur_elem_name => return Some(e),
                            (Elem::Var(Var { v: Token { t: TokenType::Id(var_name), .. }, .. }), "any") |
                            (Elem::Var(Var { v: Token { t: TokenType::Id(var_name), .. }, .. }), "var") if var_name == cur_elem_name => return Some(e),
                            _ => ()
                        }
                    }
                }
            }
            if !should_search_parent { break }
            unsafe {
                if scp_ref.scp_father.is_null() {
                    break
                } else {
                    let elem = (&*scp_ref.scp_father as &Scope).find_elem_type(t, elem_name, Option::None, true, ignore_captured);
                    if elem.is_none() { break }

                    // WARNING: this is a unsafe cast, using it to mutate a const reference
                    let scp = (self as *const Scope) as *mut Scope;
                    if let ScopeAttr::FuncScope { captured_vars, .. } = &mut (*scp).attrs {
                        if let Elem::Var(v @ Var { is_var: true, .. }) = elem.as_ref().unwrap() {
                            let not_captured = captured_vars.iter().all(|v2| v2.v.t.get_id_name() != v.v.t.get_id_name());
                            if not_captured {
                                captured_vars.push(v.clone());
                            }
                        }
                    }
                    return elem
                }
            }
        }
        Option::None
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
    pub glob_scope: Option<Box<Scope>>,
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

    fn find_elem_type<'a, 'b: 'a>(&'a self, t: &str, elem_name: &str, root_scp: Option<&'b Scope>, should_search_parent: bool) -> Option<&'a Elem> {
        let scp_ref = if root_scp.is_some() {
            root_scp.unwrap()
        } else {
            unsafe { &*self.cur_scope }
        };
        scp_ref.find_elem_type(t, elem_name, Option::None, should_search_parent, false)
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
            (StructInstance(n1), StructInstance(n2)) | (Alias(n1), Alias(n2)) if n1 == n2 => (),
            (Alias(name), t) |
            (t, Alias(name)) => {
                let elem_type = self.find_elem_type("type", &name, Option::None, true)
                                    .expect("Alias type not defined")
                                    .get_type()
                                    .get_inner_if_customtype();
                self.equivalent_types(&t, &elem_type)?;
            },
            (Pnt(inner1), Pnt(inner2)) | (PntVar(inner1), PntVar(inner2)) => {
                self.equivalent_types(&*inner1, &*inner2)?;
            },
            (Array(inner1, l1), Array(inner2, l2)) if l1 == l2 => {
                self.equivalent_types(&*inner1, &*inner2)?;
            },
            (FnType(inner1), FnType(inner2)) |
            (TupleType(inner1), TupleType(inner2)) => {
                if inner1.len() != inner2.len() {
                    return Err(VisitorError::MismatchedTypes(f.clone(), s.clone()))
                } else {
                    for (e1, e2) in inner1.iter().zip(inner2.iter()) {
                        self.equivalent_types(e1, e2)?;
                    }
                }
            },
            (UnionType(s1), UnionType(s2)) => { if s1 != s2 {
                    return Err(VisitorError::MismatchedTypes(f.clone(), s.clone()))
            } },
            (UnionType(s1), t) | (t, UnionType(s1)) => {
                let final_t = self.infer_type(&t);
                if final_t.is_unknown() {
                    return Err(VisitorError::GeneralError(format!("Unknown type '{t:?}' cannot be equivalent to UnionType({s1:?})")))
                }
                if !s1.contains(&final_t) {
                    return Err(VisitorError::MismatchedTypes(self.infer_type(f), self.infer_type(s)))
                }
            },
            (t, t2) => if t != t2 { return Err(VisitorError::MismatchedTypes(self.infer_type(f), self.infer_type(s))) }
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
            Array(inner, len) => Array(Box::new(self.infer_type_level(&**inner, level)), *len),
            Alias(name) => self.find_elem_type("type", &name, Option::None, true).expect(&format!("Alias not defined: {name}")).get_type().get_inner_if_customtype(),
            Pnt(inner) => Pnt(Box::new(self.infer_type_level(&*inner, level))),
            PntVar(inner) => PntVar(Box::new(self.infer_type_level(&*inner, level))),
            _ => t.clone()
        }
    }

    pub fn traverse(&mut self, ast: &mut Vec<Node>) -> Result<(), std::io::Error> {
        use std::io::{Error, ErrorKind};
        let mut global_scope = Box::new(Scope { attrs: ScopeAttr::GlobScope, elems: vec![], scp_father: std::ptr::null() });
        for n in &mut *ast {
            self.visit(&mut *global_scope, None, n).map_err(|e| Error::new(ErrorKind::InvalidInput, format!("Visitor Error: {e:?}")))?;
        }
        self.update_defs_types(&mut global_scope);
        self.cur_scope = &*global_scope as *const Scope;
        self.glob_scope = Some(global_scope);
        self.update_types(ast)?;
        Ok(())
    }

    fn visit(&mut self, scope: *mut Scope, expected_type: ExprType, node: &mut Node) -> Result<ExprType, VisitorError> {
        let scope = unsafe { &mut *scope };
        self.cur_scope = &*scope as *const Scope;
        match &mut *node.v {
            ASTNode::Assign { var: (is_var, ref tk @ Token { t: ref tk_type, .. }, t), expr } => {
                // if the current assignment creates an Function Scope we need to update its name
                // with the current variable name, otherwise we are creating a non-function variable
                let var_name = tk_type.clone().get_id_name().unwrap().to_string();
                self.last_def_name = Some(var_name.clone());
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
                    // Only checks if the variable already exists and is a variable if it's assignment without 'var'
                    if !*is_var {
                        if let Some(v) = self.find_elem_type("var", &var_name, Option::None, true) {
                            let def = v.get_var().clone();
                            if def.is_var {
                                self.equivalent_types(&def.t, &expression_type)?;
                                return Ok(None)
                            }
                        }
                    }
                    scope.elems.push(Elem::Var( Var {
                        is_var: *is_var,
                        v: tk.clone(),
                        t: if let CustomType(_) = expression_type { expression_type } else { self.infer_type(&expected_type)}
                    }));
                    self.last_def_name.take(); // After use, remove the last_def_name
                } else if let Elem::Scope(Scope { attrs: ScopeAttr::FuncScope { name, is_var: variable, .. }, .. }) = scope.elems.last_mut().unwrap() {
                    if &var_name != name {
                        panic!("Function name assertion failed")
                    }
                    *variable = *is_var;
                }
                Ok(None)
            },
            ASTNode::Struct(items) => {
                let mut inner_types = vec![];
                let mut scp = Scope {
                    attrs: ScopeAttr::StructScope { name: self.last_def_name.take().expect("Struct was not previously defined.") },
                    elems: vec![],
                    scp_father: scope,
                };
                for (i, item) in items.iter_mut().enumerate() {
                    let t = Unknown(self.get_unknown_id());
                    let ret_type = self.visit(&mut scp, t, item)?;
                    if let StructInstance(_) = ret_type {
                        let v = Var {
                            is_var: false,
                            // When implementing something from another struct we use a special
                            // Token to define the variable, Nl Token
                            v: Token::new_with_tk_type(0, 0, TokenType::Nl),
                            t: ret_type
                        };
                        scp.elems.push(Elem::Var(v.clone()));
                        inner_types.push(v);
                    } else {
                        inner_types.push(if let Elem::Var(v) = &scp.elems[i] {
                            v.clone()
                        } else {
                            scp.elems[i].scp_as_var()
                        });
                    }

                }
                self.last_def_name = Option::None; // Avoid that any definition inside this scope get its name used after it
                scope.elems.push(Elem::Scope(scp));
                Ok(Struct(inner_types))
            },
            ASTNode::StructInit(tk, body) => {
                let backup_assignment_name = self.last_def_name.take();
                let type_name = tk.t.get_id_name().unwrap();
                let get_var = |e: &Elem| {
                    match e {
                        Elem::Scope(scp @ Scope { attrs: ScopeAttr::TupleScope {..}, ..}) |
                        Elem::Scope(scp @ Scope { attrs: ScopeAttr::FuncScope {..}, ..}) => scp.scp_as_var(),
                        Elem::Var(v) => v.clone(),
                        _ => panic!("You should not define a non variable/function inside struct")
                    }
                };
                let defs = self.find_elem_type("struct", &type_name, Option::None, true)
                            .expect(&format!("Struct {type_name} not defined previously."))
                            .get_scp()
                            .elems.iter()
                            .map(|e| get_var(e))
                            .filter(|v| v.is_var).collect::<Vec<Var>>();
                let mut defined_scp = Scope {
                    attrs: ScopeAttr::StructScope { name: type_name.to_string() },
                    elems: vec![],
                    scp_father: std::ptr::null(), // Must not access variables from outter scopes by now
                };
                for def in body {
                    self.visit(&mut defined_scp, None, def)?;
                }

                let defined_vars = defined_scp.elems.iter().map(|e| get_var(e)).collect::<Vec<Var>>();
                if defined_vars.len() > defs.len() {
                    panic!("Received more parameters than supported, at: {}", tk)
                }
                let mut i = 0;
                for found_var in defined_vars {
                    let expected_var = &defs[i];
                    let (expected_name, found_name) = (expected_var.v.t.get_id_name().unwrap(), found_var.v.t.get_id_name().unwrap());
                    if expected_name == found_name {
                        self.equivalent_types(&expected_var.t, &found_var.t)?;
                    } else {
                        panic!("Expected assignment to field: {expected_name}, found: {found_name}")
                    }
                    i += 1;
                }
                if i != defs.len() {
                    panic!("Missing var values to be setted in Struct initialization, at: {}", tk)
                }
                self.last_def_name = backup_assignment_name;
                Ok(StructInstance(type_name.to_string()))
            },
            ASTNode::Mod(body) => {
                let mut scp = Scope {
                    attrs: ScopeAttr::ModScope { name: self.last_def_name.take().expect("Module was not previously defined.") },
                    elems: vec![],
                    scp_father: std::ptr::null(), // Must not access variables from outter scopes by now
                };

                for sttm in body {
                    self.visit(&mut scp, None, sttm)?;
                }
                self.last_def_name = Option::None; // Avoid that any definition inside this scope get its name used after it
                scope.elems.push(Elem::Scope(scp));
                Ok(None)
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
                        captured_vars: vec![],
                        is_var: false,
                    },
                    // TODO: Accept args that are not Var, like Tuples
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
                // Adding a Captured Elem for each captured Variable
                let cap_vars = if let ScopeAttr::FuncScope { captured_vars, .. } = &scope.elems.last().unwrap().get_scp().attrs {
                    captured_vars.iter().map(|v| Elem::Captured(Box::new(Elem::Var(v.clone())))).collect::<Vec<Elem>>()
                } else { unreachable!() };
                scope.elems.extend(cap_vars);

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
            ASTNode::Extern(defs) => {
                for (_, tk, ty) in defs {
                    let fn_type = ty.as_ref().unwrap().t.get_inner_type();
                    let (mut params_types, ret_type) = fn_type.split_at(fn_type.len()-1);
                    if let None = params_types[0] {
                        params_types = &[];
                    }
                    scope.elems.push(Elem::Scope(
                        Scope {
                            attrs: ScopeAttr::FuncScope {
                                name: tk.t.get_id_name().unwrap().to_string(),
                                args_len: params_types.len(),
                                ret_type: ret_type[0].clone(),
                                parent: Option::None,
                                captured_vars: vec![],
                                is_var: false
                            },
                            elems: params_types.iter().map(|t| Elem::Var(Var {
                                is_var: false,
                                v: Token::nl(0),
                                t: t.clone()
                            })).collect(),
                            scp_father: scope,
                        }
                    ));
                }
                Ok(None)
            },
            ASTNode::FnCall { caller, params, is_sttm } => {
                if let Some(name) = caller.t.get_id_name() {

                    // If the FnCall returns a function, partial application, this will be its name
                    // only creates a FuncScope with the parent_func if var_name if Some.
                    let var_name = self.last_def_name.take();
                    let scp_fn = self.find_elem_type("func", &name, Option::None, true).expect(&format!("Function '{name}' is not defined")).get_scp();
                    let fn_as_var = scp_fn.scp_as_var();
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
                        // Get the name back so a variable can be declared at Assign
                        self.last_def_name = var_name;
                        node.t = ret_type.clone();
                        ret_type.clone()
                    }
                    else if let Some(func_name) = var_name {
                        scope.elems.push(Elem::Scope(Scope {
                            attrs: ScopeAttr::FuncScope {
                                name: func_name,
                                args_len: params_types.len()-i,
                                ret_type: ret_type.clone(),
                                parent: Some(name.to_string()),
                                captured_vars: vec![],
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
                    panic!("At the moment, the caller can only be the function name or as a StrucAccess/ModAccess")
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
                        if let Real(_) = self.infer_type(&expected_type) { 
                            node.t = expected_type.clone();
                            expected_type.clone()
                        }
                        else { Real(64) }
                    },
                    TokenType::Integer(_) => {
                        if let Int{ .. } = self.infer_type(&expected_type) {
                            node.t = expected_type.clone();
                            expected_type.clone()
                        }
                        else { Int { bits: 64, signed: false } }
                    },
                    TokenType::Str(_) => Pnt(Box::new(Char)),
                    TokenType::True => Bool,
                    TokenType::False => Bool,
                    TokenType::StruAccess(name)|
                    TokenType::ModAccess(name) |
                    TokenType::Id(name) => {
                        if let Some(elem) = self.find_elem_type("any", name, Option::None, true) {
                            let t = match elem {
                                Elem::Var(v) => v.t.clone(),
                                Elem::Scope(scp @ Scope { attrs: ScopeAttr::FuncScope { .. }, .. }) => scp.scp_as_var().t,
                                Elem::Scope(Scope { attrs: ScopeAttr::TupleScope { t, .. }, .. }) => t.clone(),
                                _ => panic!("Elem {elem:?} not implemented as a Leaf.")
                            };
                            self.equivalent_types(&expected_type, &t)?;
                            self.infer_type(&expected_type)
                        }
                        else {
                            return Err(VisitorError::VariableNotDeclared(name.to_string()))
                        }
                    },
                    TokenType::PassDef => {
                        if let Scope { attrs: ScopeAttr::StructScope { .. }, .. } = scope {
                            expected_type.clone()
                        } else {
                            panic!("Skip definitions is only allowed inside a Struct definition")
                        }
                    },
                    _ => return Err(VisitorError::NotImplemented(*node.v.clone())),
                };
                self.equivalent_types(&t, &node.t)?;
                self.equivalent_types(&t, &expected_type)?;
                Ok(t)
            },
            ASTNode::Cast { e, t } => {
                let mut e_type = Unknown(self.get_unknown_id());
                e_type = self.visit(scope, e_type, e)?;
                Ok(match (&e_type, &*t) {
                    (None, None) => None,
                    (FnType(_), FnType(_)) | // At the moment it's not possible cast any function type
                    (None, _) |
                    (_, None) => {
                        return Err(VisitorError::CastError(e_type.clone(), t.clone()))
                    },
                    _ => {
                        self.equivalent_types(&expected_type, &t)?;
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
                        PntVar(inner) | Pnt(inner) | Array(inner, _) => t = *inner,
                        inner @ Unknown(_) => {
                            self.equivalent_types(&expected_type, &inner)?;
                            return Ok(self.infer_type(&expected_type))
                        },
                        _ if n == 1 => (),
                        _ => unreachable!("Tried to deref more than possible at {:?}", e.v),
                    };
                }
                self.equivalent_types(&node.t, &t)?;
                Ok(t)
            },
            ASTNode::Empty => {
                // NOTE: This should only happen when we are creating an alias of some type, a CustomType
                Ok(CustomType(Box::new(node.t.clone())))
            }
            ASTNode::Array(elems) => {
                let inner_type = Unknown(self.get_unknown_id());
                for elem in &mut *elems {
                    self.visit(scope, inner_type.clone(), elem)?;
                }
                let t = Array(Box::new(self.infer_type(&inner_type)), elems.len());
                self.equivalent_types(&t, &expected_type)?;
                node.t = t.clone();
                Ok(t)
            }
            ASTNode::Tuple(elems) => {
                let mut t = vec![];
                let tuple_type = Unknown(self.get_unknown_id());
                let mut scp = Scope {
                    attrs: ScopeAttr::TupleScope { name: self.last_def_name.take().unwrap_or("".to_string()), t: tuple_type.clone() },
                    elems: vec![],
                    scp_father: scope,
                };
                for (i, elem) in elems.iter_mut().enumerate() {
                    let tmp_t = Unknown(self.get_unknown_id());
                    self.last_def_name = Some(i.to_string());
                    t.push(self.visit(&mut scp, tmp_t.clone(), elem)?);
                    if i+1 != scp.elems.len() {
                        scp.elems.push(
                            Elem::Var(Var {
                                is_var: false,
                                v: Token::new_with_tk_type(0, 0, TokenType::Id(self.last_def_name.take().unwrap())),
                                t: tmp_t
                        }));
                    }
                }
                scope.elems.push(Elem::Scope(scp));
                self.equivalent_types(&tuple_type, &TupleType(t.clone()))?;
                node.t = self.infer_type(&tuple_type);
                Ok(self.infer_type(&tuple_type))
            },
            _ => Err(VisitorError::NotImplemented(*node.v.clone()))
        }
    }

    pub fn update_defs_types(&self, scp: &mut Scope) {
        for i in 0..scp.elems.len() {
            match &mut scp.elems[i] {
                Elem::Captured(_) => (),
                Elem::Var(v) => v.t = self.infer_type(&v.t),
                Elem::Scope(s @ Scope { attrs: ScopeAttr::TupleScope { .. }, .. }) |
                Elem::Scope(s @ Scope { attrs: ScopeAttr::FuncScope { .. }, .. }) => {
                    if let ScopeAttr::FuncScope { ret_type, .. } = &mut s.attrs {
                        *ret_type = self.infer_type(ret_type);
                    } else if let ScopeAttr::TupleScope { t, .. } = &mut s.attrs {
                        *t = self.infer_type(t);
                    }
                    self.update_defs_types(s);
                },
                Elem::Scope(s) => self.update_defs_types(s),
            }
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
                if expr.t.is_unknown() {
                    expr.t = final_type;
                }
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
            ASTNode::Array(elems) => self.update_types(elems)?,
            ASTNode::Tuple(elems) => self.update_types(elems)?,
            ASTNode::StructInit(_, body) => self.update_types(body)?,
            ASTNode::Mod(body) => self.update_types(body)?,
            ASTNode::Extern(_) => (), // TODO: possibly do something here
            _ => panic!("Not implemented yet: {ast:?}"),
        }
        if ast.t.is_unknown() {
            ast.t = self.infer_type(&ast.t);
        }
        Ok(())
    }
}
