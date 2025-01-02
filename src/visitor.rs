use crate::{grammar::{Node, ASTNode}, tokenizer::{Token, TokenType}};

use ExprType::*;
#[derive(Debug, Clone)]
pub enum ExprType {
    // Types
    I32, U32, Char, F32, F64, Bool,

    // Compounded types
    FnType(Vec<ExprType>), UnionType(Vec<ExprType>), TupleType(Vec<ExprType>),

    Void, Unknown
}
impl ExprType {
    fn expr_type_eq(f: &ExprType, s: &ExprType, strict_cmp: bool) -> bool {
        match (f, s) {
            (I32, I32)
            | (U32, U32)
            | (F32, F32)
            | (F64, F64)
            | (Char, Char)
            | (Bool, Bool)
            | (Void, Void) => true,
              (_, Unknown) if !strict_cmp => true,
              (Unknown, _) if !strict_cmp => true,
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
            ASTNode::Type { t: TokenType::I32, inner_types } if inner_types.is_empty() => I32,
            ASTNode::Type { t: TokenType::U32, inner_types } if inner_types.is_empty() => U32,
            ASTNode::Type { t: TokenType::F32, inner_types } if inner_types.is_empty() => F32,
            ASTNode::Type { t: TokenType::F64, inner_types } if inner_types.is_empty() => F64,
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
enum ScopeType {
    GlobScope,
    FuncScope,
    CondScope,
    LoopScope,
}

#[derive(Debug)]
pub struct Scope {
    scp_type: ScopeType,
    scopes: Vec<Scope>,
    vars: Vec<Var>,
    ret_type: ExprType,
}

#[derive(Debug)]
pub enum VisitorError {
    MismatchedTypes(ExprType, ExprType),
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
        let mut global_scope = Scope { scp_type: ScopeType::GlobScope, scopes: vec![], vars: vec![], ret_type: Void };
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
                        scp_type: ScopeType::FuncScope,
                        scopes: vec![],
                        vars: (0..args.len()).map(|i| Var {
                                  v: args[i].0.clone(),
                                  t: known_func_type.get_nth_inner_type(i),
                              }).collect(),
                        ret_type: known_func_type.get_fn_return_type(),
                    });
                    for node in body {
                        // TODO: Void may not be the best type to be expected, but for statements it's fine
                        self.visit(scope.scopes.last_mut().unwrap(), Void, node)?;
                    }
                    Ok(known_func_type)
                } else {
                    Err(VisitorError::MismatchedTypes(fn_type, expected_type))
                }
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
            }
            ASTNode::Leaf(tk) => {
                Ok(match tk.t {
                    TokenType::Integer => U32,
                    TokenType::Character => Char,
                    TokenType::Real => F32,
                    _ => return Err(VisitorError::NotImplemented(*node.clone())),
                })
            }
            _ => Err(VisitorError::NotImplemented(*node.clone())),
        }
    }
}
