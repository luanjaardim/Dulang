use crate::tokenizer::*;

type Body = Vec<ASTNode>;
type Node = Box<ASTNode>;

enum ASTNode {
    Assign{
        var: Token,
        expr: Node,
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
}

enum ParseError {
    TokenNotExpected(Token, TokenType),
    ExpectedToken,
    NotImplemented
}

struct Parser {
    tokenizer: core::iter::Peekable<Tokenizer>,
}

impl Parser {

    pub fn new(tokenizer: Tokenizer) -> Self {
        Parser {
            tokenizer: tokenizer.into_iter().peekable(),
        }
    }

    pub fn parse(mut self) -> Result<Vec<Node>, ParseError>  {
        let mut ast = vec![];
        while self.tokenizer.peek().is_some() {
            ast.push(self.sttm()?);
        }
        Ok(ast)
    }

    fn sttm(&mut self) -> Result<Node, ParseError> {
        match self.tokenizer.next() {
            Some(var @ Token { t: TokenType::Id, ..}) => {
                Ok(Box::new(ASTNode::Assign { var, expr: self.expr()? }))
            },
            Some(tk) => Err(ParseError::TokenNotExpected(tk, TokenType::Id)),
            None => Err(ParseError::ExpectedToken)
        }
    }

    fn expr(&mut self) -> Result<Node, ParseError> {
        self.bitwise()
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
                    l = match self.tokenizer.next() {
                        Some(tk @ Token { t: TokenType::Bxor, .. }) |
                        Some(tk @ Token { t: TokenType::Band, .. }) |
                        Some(tk @ Token { t: TokenType::Bor, .. }) |
                        Some(tk @ Token { t: TokenType::Shl, .. }) |
                        Some(tk @ Token { t: TokenType::Shr, .. }) =>
                            Box::new(
                                ASTNode::Binary {
                                    op: tk,
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
        Err(ParseError::NotImplemented)
    }
    fn arith(&mut self) -> Result<Node, ParseError> {
        Err(ParseError::NotImplemented)
    }
    fn factor(&mut self) -> Result<Node, ParseError> {
        Err(ParseError::NotImplemented)
    }
    fn unary(&mut self) -> Result<Node, ParseError> {
        Err(ParseError::NotImplemented)
    }
    fn primary(&mut self) -> Result<Node, ParseError> {
        Err(ParseError::NotImplemented)
    }


}
