use std::{error::Error, fmt::{write, Debug}};

use crate::tokenizer::*;

type Node = Box<ASTNode>;
type Body = Vec<Node>;
type Var = (Token, Option<Token>);

#[derive(Debug)]
pub enum ASTNode {
    Assign {
        var: Token, // TODO: change var type to Var, as we can define the type of a variable
        expr: Node,
    },
    Func {
        args: Vec<Var>,
        ret: Option<Token>, // any Token of a type
        body: Body,
    },
    Conditional {
        cond: Option<Node>, // Else will have a cond None
        body: Body,
        next: Option<Node>, // if it is a chained condition
    },
    Loop {
        cond: Option<Node>, // loop will have a None cond
        body: Body,
    },
    Binary {
        op: Token,
        l: Node,
        r: Node,
    },
    Unary {
        op: Token,
        e: Node,
    },
    Leaf(Token)
}

pub enum ParseError {
    TokenNotExpected(Token, Vec<TokenType>),
    ExpectedToken,
    GeneralError(String),
    NotImplemented
}

impl std::fmt::Debug for ParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::TokenNotExpected(tk, expected) => {
                write!(f, "Received {tk:?}, but expected Tokens of type {expected:?}")
            },
            Self::ExpectedToken => write!(f, "Expected a Token, but received nothing."),
            Self::GeneralError(s) => write!(f, "ParseError: {s}"),
            Self::NotImplemented => write!(f, "Still in development!"),
        }
    }
}

pub struct Parser {
    tokenizer: Tokenizer,
}

impl Parser {

    pub fn new(tokenizer: Tokenizer) -> Self {
        Parser { tokenizer }
    }

    pub fn clone_state(&self) -> Self {
        Parser { tokenizer: self.tokenizer.clone() }
    }

    fn assert_next(&mut self, possible_next_tokens_types: Vec<TokenType>) -> Result<(), ParseError> {
        let (tk, next_tk_type) = match self.tokenizer.next() {
            Some(tk @ Token { t, .. }) => (tk, t),
            None => return Err(ParseError::ExpectedToken)
        };
        if possible_next_tokens_types.iter().any(|poss_tk_type| *poss_tk_type == next_tk_type ) { return Ok(()) }
        Err(ParseError::TokenNotExpected(tk, possible_next_tokens_types))
    }

    pub fn parse(mut self) -> Result<Body, ParseError>  {
        Ok(self.body()?)
    }

    fn body(&mut self) -> Result<Body, ParseError>  {
        let mut ast = vec![];
        while self.tokenizer.peek().is_some() {
            ast.push(self.sttm()?);
        }
        Ok(ast)
    }

    fn sttm(&mut self) -> Result<Node, ParseError> {
        match self.tokenizer.next() {
            Some(var @ Token { t: TokenType::Id, ..}) => {
                match self.tokenizer.next() {
                    Some(Token { t: TokenType::Assign, .. }) =>
                        Ok(Box::new(ASTNode::Assign { var, expr: self.expr()? })),
                    Some(tk) => Err(ParseError::TokenNotExpected(tk, vec![TokenType::Assign])),
                    None => Err(ParseError::ExpectedToken)
                }
            },
            Some(tk) => Err(ParseError::TokenNotExpected(tk, vec![TokenType::Id])),
            None => Err(ParseError::ExpectedToken)
        }
    }

    fn expr(&mut self) -> Result<Node, ParseError> {
        // saving the current state of the Tokenizer
        // if the Parse fail for any branch it's easy to rollback
        let backup = self.clone_state();
        if let ret @ Ok(_) = self.bitwise() { return ret }
        *self = backup.clone_state(); // restore state of the Tokenizer
        if let ret @ Ok(_) = self.func() { return ret }

        match backup.tokenizer.peek() {
            Some(tk) =>
                Err(ParseError::GeneralError(
                    format!("Fail to parse an expression for: {}.", tk)
            )),
            None => Err(ParseError::ExpectedToken),
        }
    }

    fn func(&mut self) -> Result<Node, ParseError> {
        // Start of the function args '|'
        self.assert_next(vec![TokenType::FnBar])?;

        // TODO: parse args

        // End of the function args '|'
        self.assert_next(vec![TokenType::FnBar])?;

        // TODO: implement args parsing and return type parse
        Ok(Box::new(ASTNode::Func { args: vec![], ret: None, body: self.body()? }))
    }

    fn bitwise(&mut self) -> Result<Node, ParseError> {
        match self.tokenizer.peek() {
            Some(Token { t: TokenType::Bnot, ..})  => {
                Ok(Box::new(
                    ASTNode::Unary {
                        op: self.tokenizer.next().unwrap(),
                        e: self.bitwise()?
                }))
            }
            _ => {
                let mut l = self.comparison()?;
                loop {
                    l = match self.tokenizer.peek() {
                        Some(Token { t: TokenType::Bxor, .. }) |
                        Some(Token { t: TokenType::Band, .. }) |
                        Some(Token { t: TokenType::Bor, .. }) |
                        Some(Token { t: TokenType::Shl, .. }) |
                        Some(Token { t: TokenType::Shr, .. }) =>
                            Box::new(
                                ASTNode::Binary {
                                    op: self.tokenizer.next().unwrap(),
                                    l,
                                    r: self.comparison()?
                                }
                            ),
                        _ => break,
                    }
                }
                Ok(l)
            }
        }
    }

    fn comparison(&mut self) -> Result<Node, ParseError> {
        match self.tokenizer.peek() {
            Some(Token { t: TokenType::Not, ..})  => {
                Ok(Box::new(
                    ASTNode::Unary {
                        op: self.tokenizer.next().unwrap(),
                        e: self.comparison()?
                }))
            }
            _ => {
                let mut l = self.arith()?;
                loop {
                    l = match self.tokenizer.peek() {
                        Some(Token { t: TokenType::GrT, .. }) |
                        Some(Token { t: TokenType::GrE, .. }) |
                        Some(Token { t: TokenType::LeT, .. }) |
                        Some(Token { t: TokenType::LeE, .. }) |
                        Some(Token { t: TokenType::Neq, .. }) |
                        Some(Token { t: TokenType::Eq, .. }) =>
                            Box::new(
                                ASTNode::Binary {
                                    op: self.tokenizer.next().unwrap(),
                                    l,
                                    r: self.arith()?
                                }
                            ),
                        _ => break,
                    }
                }
                Ok(l)
            }
        }
    }

    fn arith(&mut self) -> Result<Node, ParseError> {
        let mut l = self.factor()?;
        loop {
            l = match self.tokenizer.peek() {
                Some(Token { t: TokenType::Add, .. }) |
                Some(Token { t: TokenType::Sub, .. }) =>
                    Box::new(
                        ASTNode::Binary {
                            op: self.tokenizer.next().unwrap(),
                            l,
                            r: self.factor()?
                        }
                    ),
                _ => break,
            }
        }
        Ok(l)
    }

    fn factor(&mut self) -> Result<Node, ParseError> {
        let mut l = self.unary()?;
        loop {
            l = match self.tokenizer.peek() {
                Some(Token { t: TokenType::Mul, .. }) |
                Some(Token { t: TokenType::Div, .. }) =>
                    Box::new(
                        ASTNode::Binary {
                            op: self.tokenizer.next().unwrap(),
                            l,
                            r: self.unary()?
                        }
                    ),
                _ => break,
            }
        }
        Ok(l)
    }

    fn unary(&mut self) -> Result<Node, ParseError> {
        Ok(match self.tokenizer.peek() {
            Some(Token { t: TokenType::Add, .. }) |
            Some(Token { t: TokenType::Sub, .. }) => {
                Box::new(ASTNode::Unary {
                    op: self.tokenizer.next().unwrap(),
                    e: self.unary()? 
                })
            },
            _ => self.primary()?,
        })
    }
    fn primary(&mut self) -> Result<Node, ParseError> {
        match self.tokenizer.next() {
            Some(tk @ Token { t: TokenType::Int, .. }) |
            Some(tk @ Token { t: TokenType::Str, .. }) |
            Some(tk @ Token { t: TokenType::Real, .. }) |
            Some(tk @ Token { t: TokenType::Char, .. }) => {
                Ok(Box::new(ASTNode::Leaf(tk)))
            },
            Some(Token { t: TokenType::OpParen, .. }) => {
                let expr = self.expr()?;
                match self.tokenizer.next() {
                    Some(Token { t: TokenType::ClParen, .. }) => Ok(expr),
                    Some(tk) => Err(ParseError::TokenNotExpected(tk, vec![TokenType::ClParen])),
                    None => Err(ParseError::ExpectedToken)
                }
            },
            Some(tk) => Err(ParseError::TokenNotExpected(tk, vec![
                TokenType::Real, TokenType::Int, TokenType::Char, TokenType::Str, TokenType::OpParen
            ])),
            None => Err(ParseError::ExpectedToken)
        }
    }

    fn _type_(&mut self) -> Result<Node, ParseError> {
        Err(ParseError::NotImplemented)
    }

}
