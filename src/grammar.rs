use crate::tokenizer::*;
use std::rc::Rc;
use std::io::Read;

type Node = Box<ASTNode>;
type Body = Vec<Node>;
type Var = (Token, Option<Token>);

#[derive(Debug)]
pub enum ASTNode {
    Assign {
        var: Var,
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
                write!(f, "Received {tk}, but expected Tokens of type {expected:?}")
            },
            Self::ExpectedToken => write!(f, "Expected a Token, but received nothing."),
            Self::GeneralError(s) => write!(f, "ParseError: {s}"),
            Self::NotImplemented => write!(f, "Still in development!"),
        }
    }
}

pub struct Parser {
    path: String,
    tokenizer: Tokenizer,
    prev_tk: Token,
}

impl Parser {

    pub fn new(path: &str) -> Result<Self, std::io::Error>{
        let mut f = std::fs::File::open(path)?;
        let mut s = String::new();
        f.read_to_string(&mut s)?;

        Ok(Parser {
            path: String::from(path),
            tokenizer: Tokenizer::new(Rc::new(s)),
            prev_tk: Token::nl(0), // Initial value for the previous token
        })
    }

    fn peek_tk(&self) -> Option<Token> {
        let tk = self.tokenizer.peek();
        let (l, _) = tk?.position();
        let (cur_l, _) = self.prev_tk.position();
        if l != cur_l { Some(Token::nl(l)) } else { self.tokenizer.peek() }
    }

    fn next_tk(&mut self) -> Option<Token> {
        if let tk @ Token { t: TokenType::Nl, .. } = self.peek_tk()? {
            self.prev_tk = tk;
        } else {
            self.prev_tk = self.tokenizer.next()?;
        }
        // The None value is handled with the '?' in self.tokenizer.next()
        Some(self.prev_tk.clone())
    }

    /// Verifies if the next token type is one of the `possible_next_tokens`
    fn assert_peek(&mut self, possible_next_tokens_types: &[TokenType]) -> Result<Token, ParseError> {
        let (tk, next_tk_type) = match self.peek_tk() {
            Some(tk @ Token { t, .. }) => (tk, t),
            None => return Err(ParseError::ExpectedToken)
        };
        if possible_next_tokens_types.iter().any(|poss_tk_type| *poss_tk_type == next_tk_type ) { return Ok(tk) }
        Err(ParseError::TokenNotExpected(tk, possible_next_tokens_types.to_vec()))
    }

    /// Advances the Tokenizer cursor if the next token type is one of the `possible_next_tokens`
    fn assert_next(&mut self, possible_next_tokens_types: &[TokenType]) -> Result<Token, ParseError> {
        let ret = self.assert_peek(possible_next_tokens_types)?;
        self.next_tk();
        Ok(ret)
    }

    /// Function to parse chained binary expressions, such as:
    ///     factor ((ADD | SUB) factor)*  =>  1 + 1 - 2 + 1 + 3
    ///     comparison ( (BAND | BOR | BXOR | SHL | SHR) comparison )*  =>  1 band 2 bor 0
    fn parse_generic_chained_binary(
        &mut self,
        mut method: impl FnMut(&mut Self) -> Result<Node, ParseError>,
        possible_operators: Vec<TokenType>
    ) -> Result<Node, ParseError> {
        let mut l = method(self)?;
        loop {
            let op = self.assert_peek(&possible_operators);
            l = match op {
                Ok(tk) => {
                    _ = self.next_tk(); // Consume the Token peeked in assert_peek
                    Box::new(ASTNode::Binary { op: tk, l, r: method(self)? })
                }
                Err(_) => break,
            };
        }
        Ok(l)
    }

    /// Parse something between two Nl (new lines '\n'), they may not exist also
    fn parse_and_discart_nl<T: std::fmt::Debug>(
        &mut self,
        mut method: impl FnMut(&mut Self) -> Result<T, ParseError>,
    ) -> Result<T, ParseError>  {
        _ = self.assert_next(&[TokenType::Nl]);
        let ret = method(self);
        _ = self.assert_next(&[TokenType::Nl]);
        ret
    }

    pub fn parse(mut self) -> Result<Body, std::io::Error>  {
        use std::io::{Error, ErrorKind};
        match self.body() {
            Err(e) => Err(Error::new(ErrorKind::InvalidInput, format!("({}) {e:?}", self.prev_tk))),
            Ok(e) if self.peek_tk().is_none() =>  Ok(e),
            _ =>  Err(Error::new(ErrorKind::InvalidInput, format!("({}) Failed to parse body.", self.prev_tk))),
        }
    }

    fn body(&mut self) -> Result<Body, ParseError>  {
        let mut ast = vec![];
        while let Ok(node) = self.parse_and_discart_nl(|s| s.sttm()) { ast.push(node); }
        Ok(ast)
    }

    /// Parse a list of statements inside curly brackets.
    fn inner_body(&mut self) -> Result<Body, ParseError>  {
        _ = self.parse_and_discart_nl(|s| Ok(s.assert_next(&[TokenType::OpCurly])?))?;
        let body = self.body()?;
        _ = self.parse_and_discart_nl(|s| Ok(s.assert_next(&[TokenType::ClCurly])?))?;
        Ok(body)
    }

    fn sttm(&mut self) -> Result<Node, ParseError> {
        match self.peek_tk() {
            // Var definition
            Some(Token { t: TokenType::Id, ..}) => {
                let var = self.var()?;
                _ = self.assert_next(&[TokenType::Assign])?;
                Ok(Box::new(ASTNode::Assign { var, expr: self.expr()? }))
            },
            // Cond as statement
            Some(Token { t: TokenType::If, ..}) => self.cond(),
            // Loop statement
            Some(Token { t: TokenType::Loop, ..}) |
            Some(Token { t: TokenType::While, ..}) => self._loop_(),
            Some(tk) => Err(ParseError::TokenNotExpected(tk, vec![TokenType::Id])),
            None => Err(ParseError::ExpectedToken)
        }
    }

    fn _loop_(&mut self) -> Result<Node, ParseError> {
        _ = self.assert_peek(&[TokenType::While, TokenType::Loop])?;
        Ok(Box::new(ASTNode::Loop {
            cond: match self.next_tk() {
                Some(Token { t: TokenType::While, .. }) => Some(self.expr()?),
                Some(Token { t: TokenType::Loop, .. }) => None,
                _ => return Err(ParseError::NotImplemented)  // TODO: Implement For loop
            },
            body: self.inner_body()? }))
    }

    fn cond(&mut self) -> Result<Node, ParseError> {
        _ = self.assert_next(&[TokenType::If])?;
        Ok(Box::new(ASTNode::Conditional { 
            cond: self.comparison().ok(),
            body: self.inner_body()?,
            next: match self.peek_tk() {
                Some(Token { t: TokenType::Elif, .. }) => Some(self.elif()?),
                Some(Token { t: TokenType::Else, .. }) => Some(self._else_()?),
                _ => None,
            },
        }))
    }

    fn elif(&mut self) -> Result<Node, ParseError> {
        _ = self.assert_next(&[TokenType::Elif])?;
        Ok(Box::new(ASTNode::Conditional { 
            cond: self.comparison().ok(),
            body: self.inner_body()?,
            next: match self.peek_tk() {
                Some(Token { t: TokenType::Elif, .. }) => Some(self.elif()?),
                Some(Token { t: TokenType::Else, .. }) => Some(self._else_()?),
                _ => None,
            },
        }))
    }

    fn _else_(&mut self) -> Result<Node, ParseError> {
        _ = self.assert_next(&[TokenType::Else])?;
        Ok(Box::new(ASTNode::Conditional { cond: None, body: self.inner_body()?, next: None, }))
    }

    fn expr(&mut self) -> Result<Node, ParseError> {
        // Saving the current state of the Tokenizer
        // if the Parse fail for any branch it's easy to rollback
        let backup = self.tokenizer.get_state();

        if let ret @ Ok(_) = self.bitwise() { return ret }
        self.tokenizer.set_state(backup); // restore state of the Tokenizer
        if let ret @ Ok(_) = self.func() { return ret }
        self.tokenizer.set_state(backup); // restore state of the Tokenizer
        if let ret @ Ok(_) = self.cond() { return ret }

        match self.peek_tk() {
            Some(tk) =>
                Err(ParseError::GeneralError(
                    format!("Fail to parse an expression for: {}.", tk)
            )),
            None => Err(ParseError::ExpectedToken),
        }
    }

    fn func(&mut self) -> Result<Node, ParseError> {
        // Start of the function args '|'
        _ = self.assert_next(&[TokenType::FnBar])?;

        let mut args = vec![];
        // Take args as function parameters till find a FnBar: '|'
        while self.assert_peek(&[TokenType::FnBar]).is_err() {
            args.push(self.var()?);
            if self.assert_peek(&[TokenType::Comma]).is_ok() { _ = self.next_tk(); }
        }

        // End of the function args '|'
        _ = self.assert_next(&[TokenType::FnBar])?;

        // TODO: Change types to be an ASTNode, as a complex type would need more
        // than a Token to be represented
        let ret = if self.assert_next(&[TokenType::FnReturn]).is_ok() {
            Some(self._type_()?)
        } else { None };

        Ok(Box::new(ASTNode::Func { args, ret, body: self.inner_body()? }))
    }

    fn var(&mut self) -> Result<Var, ParseError> {
        Ok((
            self.assert_next(&[TokenType::Id])?,
            match self.peek_tk() {
                Some(Token { t: TokenType::TypeInf, .. }) => {
                    _ = self.next_tk();
                    Some(self._type_()?)
                },
                _ => None
            }
        ))
    }

    fn bitwise(&mut self) -> Result<Node, ParseError> {
        match self.peek_tk() {
            Some(Token { t: TokenType::Bnot, ..})  => {
                Ok(Box::new(
                    ASTNode::Unary {
                        op: self.next_tk().unwrap(),
                        e: self.bitwise()?
                }))
            }
            _ => {
                Ok(self.parse_generic_chained_binary(|s| s.comparison(), vec![
                        TokenType::Bxor,
                        TokenType::Band,
                        TokenType::Bor,
                        TokenType::Shl,
                        TokenType::Shr,
                ])?)
            }
        }
    }

    fn comparison(&mut self) -> Result<Node, ParseError> {
        match self.peek_tk() {
            Some(Token { t: TokenType::Not, ..})  => {
                Ok(Box::new(
                    ASTNode::Unary {
                        op: self.next_tk().unwrap(),
                        e: self.comparison()?
                }))
            }
            _ => {
                Ok(self.parse_generic_chained_binary(|s| s.arith(), vec![
                        TokenType::GrT,
                        TokenType::GrE,
                        TokenType::LeT,
                        TokenType::LeE,
                        TokenType::Neq,
                        TokenType::And,
                        TokenType::Or,
                        TokenType::Eq,
                ])?)
            }
        }
    }

    fn arith(&mut self) -> Result<Node, ParseError> {
        Ok(self.parse_generic_chained_binary(
            |s| s.factor(),
            vec![ TokenType::Add, TokenType::Sub ]
        )?)
    }

    fn factor(&mut self) -> Result<Node, ParseError> {
        Ok(self.parse_generic_chained_binary(
            |s| s.unary(),
            vec![ TokenType::Mul, TokenType::Div ]
        )?)
    }

    fn unary(&mut self) -> Result<Node, ParseError> {
        Ok(match self.peek_tk() {
            Some(Token { t: TokenType::Add, .. }) |
            Some(Token { t: TokenType::Sub, .. }) => {
                Box::new(ASTNode::Unary {
                    op: self.next_tk().unwrap(),
                    e: self.unary()? 
                })
            },
            _ => self.primary()?,
        })
    }
    fn primary(&mut self) -> Result<Node, ParseError> {
        match self.peek_tk() {
            Some(tk @ Token { t: TokenType::Id, .. }) |
            Some(tk @ Token { t: TokenType::Str, .. }) |
            Some(tk @ Token { t: TokenType::Real, .. }) |
            Some(tk @ Token { t: TokenType::Integer, .. }) |
            Some(tk @ Token { t: TokenType::Character, .. }) => {
                _ = self.next_tk(); // Discart prev Token
                Ok(Box::new(ASTNode::Leaf(tk)))
            },
            Some(Token { t: TokenType::OpParen, .. }) => {
                _ = self.assert_next(&[TokenType::OpParen])?; // Discart prev Token
                let expr = self.expr()?;
                match self.next_tk() {
                    Some(Token { t: TokenType::ClParen, .. }) => Ok(expr),
                    Some(tk) => Err(ParseError::TokenNotExpected(tk, vec![TokenType::ClParen])),
                    None => Err(ParseError::ExpectedToken)
                }
            },
            Some(tk) => Err(ParseError::TokenNotExpected(tk, vec![
                TokenType::Real, TokenType::Integer, TokenType::Character, TokenType::Str, TokenType::OpParen
            ])),
            None => Err(ParseError::ExpectedToken)
        }
    }

    fn _type_(&mut self) -> Result<Token, ParseError> {
        self.assert_next(&[
            TokenType::I32,
            TokenType::U32,
            TokenType::Char,
            TokenType::F32,
            TokenType::F64,
            TokenType::Bool,
        ])
    }

}
