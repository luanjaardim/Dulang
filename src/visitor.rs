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
fn expr_type_eq(f: &ExprType, s: &ExprType) -> bool {
    match (f, s) {
        (I32, I32)
        | (U32, U32)
        | (F32, F32)
        | (F64, F64)
        | (Char, Char)
        | (Bool, Bool)
        | (Void, Void)
        | (Unknown, Unknown) => true,
        (FnType(l1), FnType(l2))
        | (UnionType(l1), UnionType(l2))
        | (TupleType(l1), TupleType(l2)) => {
            if l1.len() != l2.len() { return false }
            l1.iter().enumerate().any(|(i, e)| expr_type_eq(e, &l2[i]))
        },
        _ => false,
    }
}
impl PartialEq for ExprType {
    fn eq(&self, other: &Self) -> bool {
        expr_type_eq(self, other)
    }
    fn ne(&self, other: &Self) -> bool {
        !expr_type_eq(self, other)
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
            self.visit(&mut global_scope, n).map_err(|e| Error::new(ErrorKind::InvalidInput, format!("Visitor Error: {e:?}")))?;
        }
        self.ast = ast;
        self.glob_scope = Some(global_scope);
        Ok(())
    }

    fn visit(&mut self, scope: &mut Scope, node: &Node) -> Result<ExprType, VisitorError> {
        match &**node {
            ASTNode::Assign { var: (v, expected_type), expr: expression } => {
                let expr_type = self.visit(scope, expression)?;
                scope.vars.push(Var {
                    v: v.clone(),
                    t: (
                        if let Some(t_node) = expected_type {
                            let t = ExprType::from(t_node);
                            if t != expr_type {
                                return Err(VisitorError::MismatchedTypes(t, expr_type));
                            } else { t }
                        } else { expr_type }
                    ),
                });
                Ok(Void)
            },
            ASTNode::Func { args, ret, body } => {
                let args_vars: Vec<Var> = args.iter().map(|arg| Var {
                    v: arg.0.clone(),
                    t: arg.1.as_ref().map_or(Unknown, |t| ExprType::from(t)),
                }).collect();

                scope.scopes.push(Scope {
                    scp_type: ScopeType::FuncScope,
                    scopes: vec![],
                    vars: args_vars,
                    ret_type: ExprType::from(ret.as_ref().map_or(Unknown, |t| ExprType::from(t))),
                });
                for node in body {
                    self.visit(scope.scopes.last_mut().unwrap(), node)?;
                }
                let func_scope = scope.scopes.last().unwrap();
                Ok(FnType(
                    (0..args.len())
                        .map(|i| func_scope.vars[i].t.clone())
                        .chain([func_scope.ret_type.clone()])
                        .collect()
                ))
            },
            ASTNode::Leaf(tk) => {
                Ok(match tk.t {
                    TokenType::Integer => U32,
                    TokenType::Character => Char,
                    _ => return Err(VisitorError::NotImplemented(*node.clone())),
                })
            }
            _ => Err(VisitorError::NotImplemented(*node.clone())),
        }
    }
}
