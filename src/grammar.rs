use crate::{tokenizer::*, visitor::ExprType::{self, Unknown}};
// use crate::tokenizer::*;
use std::io::Read;
use std::rc::Rc;

// use ExprType::{Void, Unknown};
// #[derive(Debug, Clone)]
// pub enum ExprType {
//     Unknown(usize),
//     Void
// }

#[derive(Clone)]
pub struct Node {
    pub t: ExprType,
    pub v: InnerNode
}
impl Node {
    pub fn new(t: ExprType, v: InnerNode) -> Self { Node { t, v } }
}
impl std::fmt::Debug for Node {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "(t: {:#?}, ", self.t)?;
        write!(f, "v: {:#?})", self.v)
    }
}

pub type InnerNode = Box<ASTNode>;
type Body = Vec<Node>;
type Var = (bool, Token, Option<Node>);

#[derive(Debug, Clone)]
pub enum ASTNode {
    Assign {
        var: Var,
        expr: Node,
    },
    Mod {
        name: String,
        body: Body,
    },
    Struct(Body),
    StructInit(Token, Body),
    Func {
        args: Vec<Var>,
        ret: ExprType,
        body: Body,
    },
    Conditional {
        cond: Option<Node>, // Else will have a cond None
        body: Body,
        next: Option<Node>, // If it is a chained condition
    },
    Loop {
        cond: Option<Node>, // Loop will have a None cond
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
    FnCall {
        caller: Token,
        params: Vec<Node>,  // List of expressions
        is_sttm: bool,
    },
    Type {
        t: TokenType,
        inner_types: Vec<InnerNode>,
    },
    Cast {
        e: Node,
        t: ExprType
    },
    Array(Body),
    Tuple(Body),
    Ref {
        var: bool,
        e: Node,
    },
    Deref {
        n: usize,  // number of derefs
        e: Node,
        i: Option<Node>, // when it's a Deref with syntax: e @ i
    },
    // skip, stop or back, only back and stop can have the second element (the expr they return)
    FlowChange(TokenType, Option<Node>),
    Leaf(Token),
    Empty
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
    pub unknown_id: usize,
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
            unknown_id: 0,
        })
    }

    fn get_unknown_id(&mut self) -> usize {
        self.unknown_id += 1;
        self.unknown_id
    }

    fn get_state(&self) -> (Token, usize) {
        (self.prev_tk.clone(), self.tokenizer.get_state())
    }
    fn set_state(&mut self, state: &(Token, usize)) {
        self.prev_tk = state.0.clone();
        self.tokenizer.set_state(state.1);
    }

    fn peek_tk(&mut self) -> Option<Token> {
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
            Some(ref tk @ Token { ref t, .. }) => (tk.clone(), t.clone()),
            None => return Err(ParseError::ExpectedToken)
        };
        if possible_next_tokens_types.iter().any(|poss_tk_type| *poss_tk_type == next_tk_type) { return Ok(tk) }
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
                    Node::new(Unknown(self.get_unknown_id()), Box::new(ASTNode::Binary { op: tk, l, r: method(self)? }))
                }
                Err(_) => break,
            };
        }
        Ok(l)
    }

    /// Parse something between two Nl (new lines '\n'), they may not exist also
    fn parse_and_discart_nl<T>(
        &mut self,
        method: impl Fn(&mut Self) -> Result<T, ParseError>,
    ) -> Result<T, ParseError>  {
        _ = self.assert_next(&[TokenType::Nl]);
        method(self)
    }

    pub fn parse(mut self, last_unknown: &mut usize) -> Result<Body, std::io::Error>  {
        use std::io::{Error, ErrorKind};
        let ret = match self.body(None) {
            Err(e) => Err(Error::new(ErrorKind::InvalidInput, format!("({}) {e:?}", self.prev_tk))),
            Ok(e) if self.peek_tk().is_none() =>  Ok(e),
            _ =>  Err(Error::new(ErrorKind::InvalidInput, format!("({}) Failed to parse body.", self.prev_tk))),
        };
        *last_unknown = self.unknown_id;
        ret
    }

    fn body_aux<T>(
        &mut self,
        line_end: Option<TokenType>,
        method: impl Fn(&mut Self) -> Result<T, ParseError>,
    ) -> Result<Vec<T>, ParseError>  {
        let mut ast = vec![];
        while let Ok(node) = self.parse_and_discart_nl(&method) {
            ast.push(node);
            match (&line_end, self.peek_tk()) {
                (Some(end), Some(Token { t, .. })) if *end == t => _ = self.next_tk(),
                (Some(_), _) => return Ok(ast),
                (None, _) => (),
            }
        }
        Ok(ast)
    }

    fn body(&mut self, line_end: Option<TokenType>) -> Result<Body, ParseError>  {
        self.body_aux(line_end, |s| s.sttm())
    }

    fn inner_body_aux(&mut self, line_end: Option<TokenType>, first: TokenType, second: TokenType) -> Result<Body, ParseError>  {
        _ = self.parse_and_discart_nl(|s| Ok(s.assert_next(&[first.clone()])?))?;
        let body = self.body(line_end)?;
        _ = self.parse_and_discart_nl(|s| Ok(s.assert_next(&[second.clone()])?))?;
        Ok(body)
    }

    /// Parse a list of statements inside curly brackets.
    fn inner_body(&mut self, line_end: Option<TokenType>) -> Result<Body, ParseError>  {
        self.inner_body_aux(line_end, TokenType::OpCurly, TokenType::ClCurly)
    }

    fn parse_with_delim_and_end_line<T>(
        &mut self,
        line_end: Option<TokenType>,
        first: TokenType,
        second: TokenType,
        method: impl Fn(&mut Self) -> Result<T, ParseError>,
    ) -> Result<Vec<T>, ParseError> {
        _ = self.parse_and_discart_nl(|s| Ok(s.assert_next(&[first.clone()])?))?;
        let body = self.body_aux(line_end, method)?;
        _ = self.parse_and_discart_nl(|s| Ok(s.assert_next(&[second.clone()])?))?;
        Ok(body)
    }

    fn sttm(&mut self) -> Result<Node, ParseError> {
        match self.peek_tk() {
            // Var definition
            Some(Token { t: TokenType::StruAccess(_), ..})  |
            Some(Token { t: TokenType::ModAccess(_), ..})  |
            Some(Token { t: TokenType::Id(_), ..})  |
            Some(Token { t: TokenType::VarDef, ..}) => {
                let backup = self.get_state(); // Return to before the var if it's not an assignment

                // Module definition
                if let ret @ Ok(_) = self.module() { return ret }
                self.set_state(&backup);
                if let ret @ Ok(_) = self.assign() { return ret }
                self.set_state(&backup);
                if let ret @ Ok(_) = self.fn_call(true, None) { return ret }
                self.set_state(&backup);

                return Err(ParseError::GeneralError(format!("Could not parse sttmt at {}", self.peek_tk().unwrap())))
            },
            // Cond as statement
            Some(Token { t: TokenType::If, ..}) => self.cond(),
            // Loop statement
            Some(Token { t: TokenType::Loop, ..}) |
            Some(Token { t: TokenType::While, ..}) => self._loop_(),
            Some(Token { t: TokenType::Back, ..}) |
            Some(Token { t: TokenType::Stop, ..}) => {
                let tk = self.assert_next(&[ TokenType::Back, TokenType::Stop ])?;
                let e = self.expr().ok();
                Ok(Node::new(ExprType::None, Box::new(ASTNode::FlowChange(tk.t, e))))
            },
            Some(Token { t: TokenType::Skip, ..}) => {
                let tk = self.assert_next(&[TokenType::Skip])?;
                Ok(Node::new(ExprType::None, Box::new(ASTNode::FlowChange(tk.t, None))))
            },
            Some(tk) => Err(ParseError::TokenNotExpected(tk, vec![TokenType::Id(String::new())])),
            None => Err(ParseError::ExpectedToken)
        }
    }

    fn module(&mut self) -> Result<Node, ParseError> {
        let mod_name = if let Token { t: TokenType::Id(name), .. } = self.assert_next(&[TokenType::Id(String::new())])? {
            name
        } else { unreachable!() };
        _ = self.assert_next(&[TokenType::Assign])?;
        _ = self.assert_next(&[TokenType::Mod])?;
        let body = match self.peek_tk() {
            Some(Token { t: TokenType::ModAccess(mod_path), .. }) => {
                _ = self.next_tk();
                let path = mod_path.replace(':', "/");
                println!("Importing file: {path}");
                Parser::new(&path)
                    .expect(format!("Could not create a parser of the file {path}.").as_str())
                    .parse(&mut self.unknown_id)
                    .expect(format!("Could not parse the file {path}.").as_str())
            },
            Some(_) => self.inner_body(None)?,
            None => return Err(ParseError::ExpectedToken),
        };
        Ok(Node::new(ExprType::None, Box::new(ASTNode::Mod { name: mod_name, body })))
    }

    fn _loop_(&mut self) -> Result<Node, ParseError> {
        Ok(Node::new(ExprType::None, Box::new(ASTNode::Loop {
            cond: match self.next_tk() {
                Some(Token { t: TokenType::While, .. }) => Some(self.expr()?),
                Some(Token { t: TokenType::Loop, .. }) => None,
                _ => return Err(ParseError::NotImplemented)  // TODO: Implement For loop
            },
            body: self.inner_body(None)? })))
    }

    fn cond(&mut self) -> Result<Node, ParseError> {
        _ = self.assert_next(&[TokenType::If])?;
        Ok(Node::new(ExprType::None, Box::new(ASTNode::Conditional { 
            cond: self.expr().ok(),
            body: self.inner_body(None)?,
            next: match self.peek_tk() {
                Some(Token { t: TokenType::Elif, .. }) => Some(self.elif()?),
                Some(Token { t: TokenType::Else, .. }) => Some(self._else_()?),
                _ => None,
            },
        })))
    }

    fn elif(&mut self) -> Result<Node, ParseError> {
        _ = self.assert_next(&[TokenType::Elif])?;
        Ok(Node::new(ExprType::None, Box::new(ASTNode::Conditional { 
            cond: self.expr().ok(),
            body: self.inner_body(None)?,
            next: match self.peek_tk() {
                Some(Token { t: TokenType::Elif, .. }) => Some(self.elif()?),
                Some(Token { t: TokenType::Else, .. }) => Some(self._else_()?),
                _ => None,
            },
        })))
    }

    fn _else_(&mut self) -> Result<Node, ParseError> {
        _ = self.assert_next(&[TokenType::Else])?;
        Ok(Node::new(ExprType::None, Box::new(ASTNode::Conditional { cond: None, body: self.inner_body(None)?, next: None, })))
    }

    fn assign(&mut self) ->  Result<Node, ParseError> {
        let var = self.var()?;
        _ = self.assert_next(&[TokenType::Assign])?;
        let expr = self.expr()?;
        Ok(Node::new(ExprType::None, Box::new(ASTNode::Assign { var, expr })))
    }

    fn fn_call(&mut self, is_sttm: bool, last_arg: Option<Node>) -> Result<Node, ParseError> {
        let backup = self.get_state();
        let e =  self.assert_next(&[TokenType::Id(String::new()), TokenType::ModAccess(String::new()), TokenType::StruAccess(String::new())]);
        let tk = e?;
        if self.assert_next(&[TokenType::None]).is_ok() {
            return Ok(Node::new(Unknown(self.get_unknown_id()), Box::new(ASTNode::FnCall { caller: tk, params: vec![], is_sttm })))
        }
        let mut b = backup.clone();
        let mut params = Vec::new();
        loop {
            match self.peek_tk() {
                Some(Token { t: TokenType::PassR, .. }) => {
                    _ = self.next_tk();
                    let t = Unknown(self.get_unknown_id());
                    if let Some(last) = last_arg { params.push(last) }
                    return self.fn_call(is_sttm, Some(
                            Node::new(t, Box::new(ASTNode::FnCall { caller: tk, params, is_sttm: false })
                        )));
                },
                Some(Token { t: TokenType::PassL, .. }) => {
                    _ = self.next_tk();
                    if let Some(last) = last_arg { params.push(last) }
                    let next_fn = self.fn_call(false, None)?;
                    params.push(next_fn);
                    return Ok(Node::new(Unknown(self.get_unknown_id()),
                            Box::new(ASTNode::FnCall { caller: tk, params, is_sttm })
                    ))
                }
                Some(first_tk) => {
                    match self.expr() {
                        Ok(e) => {
                            match *e.v {
                                // Only accepts its parameters if none of them is a Unary that did not started with '('
                                // This caused (a + 1) or (a - 1) to be parsed as a function call, when it should be a binary operation
                                ASTNode::Deref { .. } |
                                ASTNode::Unary { .. } => {
                                    if first_tk.t != TokenType::OpParen {
                                        params.clear();
                                        self.set_state(&backup);
                                        break
                                    }
                                },
                                _ => (),
                            }
                            params.push(e);
                            b = self.get_state();
                        },
                        _ => {
                            self.set_state(&b);
                            break
                        }
                    }
                }
                _ => {
                    println!("Gone into None Token found in grammar fn_call function");
                    break
                }
            }
        }
        if params.is_empty() {
            return Err(ParseError::GeneralError("Expected expressions as Function Call Parameters".to_owned()))
        }
        if let Some(last) = last_arg { params.push(last) }
        Ok(Node::new(Unknown(self.get_unknown_id()), Box::new(ASTNode::FnCall { caller: tk, params, is_sttm })))
    }

    fn expr(&mut self) -> Result<Node, ParseError> {
        let e = self.parsed_expr()?;

        Ok(if let Ok(_) = self.assert_peek(&[TokenType::TypeInf]) {
            _ = self.assert_next(&[TokenType::TypeInf])?;

            let t = self._type_()?;
            Node::new(ExprType::None, Box::new(ASTNode::Cast { e, t: t.t }))
        } else {
            e
        })
    }

    fn parsed_expr(&mut self) -> Result<Node, ParseError> {
        // Saving the current state of the Tokenizer
        // if the Parse fail for any branch it's easy to rollback
        let backup = self.get_state();

        if let ret @ Ok(_) = self._struct_() { return ret }
        self.set_state(&backup); // restore state of the Tokenizer
        if let ret @ Ok(_) = self.array() { return ret }
        self.set_state(&backup); // restore state of the Tokenizer
        if let ret @ Ok(_) = self.func() { return ret }
        self.set_state(&backup); // restore state of the Tokenizer
        if let ret @ Ok(_) = self._type_() {
            match &ret.as_ref().unwrap().t {
                t if t.is_alias() => println!("Ignore type if it's only a single alias"),
                ExprType::Pnt(inner) | ExprType::PntVar(inner) if inner.is_alias() => {
                    let after_type = self.get_state();
                    self.set_state(&backup);
                    if let var_ref @ Ok(_) = self.unary() {
                        return var_ref
                    } else {
                        self.set_state(&after_type);
                        return ret
                    }
                },
                _ => return ret,
            }
        }
        if let ret @ Ok(_) = self.sub_expr(backup) { return ret }

        match self.peek_tk() {
            Some(tk) =>
                Err(ParseError::GeneralError(
                    format!("Fail to parse an expression for: {}.", tk)
            )),
            None => Err(ParseError::ExpectedToken),
        }
    }

    fn sub_expr(&mut self, backup: (Token, usize)) -> Result<Node, ParseError> {
        self.set_state(&backup); // restore state of the Tokenizer
        if let ret @ Ok(_) = self.fn_call(false, None) { return ret }
        self.set_state(&backup); // restore state of the Tokenizer
        if let ret @ Ok(_) = self.unary() { return ret }
        self.set_state(&backup); // restore state of the Tokenizer
        if let ret @ Ok(_) = self.bitwise() { return ret }
        Err(ParseError::GeneralError("Sub Expression not found".to_string()))
    }

    fn _struct_(&mut self) -> Result<Node, ParseError> {
        _ = self.parse_and_discart_nl(|s| Ok(s.assert_next(&[TokenType::OpCurly])?))?;
        let body = self._struct_body()?;
        _ = self.parse_and_discart_nl(|s| Ok(s.assert_next(&[TokenType::ClCurly])?))?;
        Ok(body)
    }

    fn _struct_body(&mut self) -> Result<Node, ParseError> {
        let mut ast = vec![];
        while let Ok(node) = self.parse_and_discart_nl(|s| {
            let n = s.assign();
            _ = s.assert_next(&[TokenType::Semicolon])?;
            n
        }) { ast.push(node); }
        Ok(Node::new(Unknown(self.get_unknown_id()), Box::new(ASTNode::Struct(ast))))
    }

    fn array(&mut self) -> Result<Node, ParseError> {
        Ok(Node::new(
                Unknown(self.get_unknown_id()),
                Box::new(ASTNode::Array(self.parse_with_delim_and_end_line(
                    Some(TokenType::Comma),
                    TokenType::OpSqrBra,
                    TokenType::ClSqrBra,
                    |s| s.expr()
                )?))
        ))
    }

    fn func(&mut self) -> Result<Node, ParseError> {
        // Start of the function args '('
        _ = self.assert_next(&[TokenType::OpParen])?;

        let mut args = vec![];
        // Take args as function parameters till find a ClParen: ')'
        while self.assert_peek(&[TokenType::ClParen]).is_err() {
            args.push(self.var()?);
            if self.assert_peek(&[TokenType::Comma]).is_ok() { _ = self.next_tk(); }
        }

        // End of the function args ')'
        _ = self.assert_next(&[TokenType::ClParen])?;

        _ = self.assert_peek(&[TokenType::Colon]);
        let ret = if self.assert_peek(&[TokenType::Colon]).is_ok() { Unknown(self.get_unknown_id()) }
        else { self._type_()?.t };

        // Starting the function body after the Colon: ':'
        _ = self.assert_next(&[TokenType::Colon])?;

        Ok(Node::new(Unknown(self.get_unknown_id()), Box::new(ASTNode::Func { args, ret, body: self.inner_body(None)? })))
    }

    fn var(&mut self) -> Result<Var, ParseError> {
        Ok((
            self.assert_next(&[TokenType::VarDef]).is_ok(),
            self.assert_next(&[TokenType::Id(String::new())])?,
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
                Ok(Node::new(Unknown(self.get_unknown_id()), Box::new(
                    ASTNode::Unary {
                        op: self.next_tk().unwrap(),
                        e: self.bitwise()?
                })))
            },
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
                Ok(Node::new(Unknown(self.get_unknown_id()), Box::new(
                    ASTNode::Unary {
                        op: self.next_tk().unwrap(),
                        e: self.comparison()?
                })))
            }
            _ => {
                Ok(self.parse_generic_chained_binary(|s| s.indexing(), vec![
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

    fn indexing(&mut self) -> Result<Node, ParseError> {
        let mut e = self.arith()?;
        loop {
             match self.assert_next(&[TokenType::Deref]) {
                Ok(_) => {
                    let i = self.arith().ok();
                    let is_some = i.is_some();
                    e = Node::new(Unknown(self.get_unknown_id()),
                          Box::new(ASTNode::Deref { n: 1, e, i })
                    );
                    if is_some { continue } else { return Ok(e) }
                },
                _ => break,
            }
        }
        Ok(e)
    }

    fn arith(&mut self) -> Result<Node, ParseError> {
        Ok(self.parse_generic_chained_binary(
            |s| s.factor(),
            vec![ TokenType::Add, TokenType::Sub ]
        )?)
    }

    fn factor(&mut self) -> Result<Node, ParseError> {
        Ok(self.parse_generic_chained_binary(
            |s| s.primary(),
            vec![ TokenType::Mul, TokenType::Div ]
        )?)
    }

    fn unary(&mut self) -> Result<Node, ParseError> {
        match self.peek_tk() {
            Some(Token { t: TokenType::Add, .. }) |
            Some(Token { t: TokenType::Sub, .. }) => {
                Ok(Node::new(Unknown(self.get_unknown_id()), Box::new(ASTNode::Unary {
                    op: self.next_tk().unwrap(),
                    e: self.primary()? 
                })))
            },
            Some(Token { t: t @ TokenType::Ref, .. }) |
            Some(Token { t: t @ TokenType::VarRef, .. }) => {
                let inner_t = Unknown(self.get_unknown_id());
                let is_var = t == TokenType::VarRef;
                _ = self.next_tk();
                Ok(Node::new(
                    if is_var {
                        ExprType::PntVar(Box::new(inner_t))
                    } else {
                        ExprType::Pnt(Box::new(inner_t))
                    }, Box::new(
                        ASTNode::Ref { var: is_var, e: self.primary()? }
                    )))
            },
            Some(Token { t: TokenType::Deref, .. }) => {
                let mut n = 0;
                while let Some(Token { t: TokenType::Deref, .. }) = self.peek_tk() {
                    n += 1;
                    _ = self.next_tk();
                }
                Ok(Node::new(Unknown(self.get_unknown_id()), Box::new(
                        ASTNode::Deref { n, e: self.primary()?, i: None }
                    )))
            },
            Some(tk) => Err(ParseError::TokenNotExpected(tk, vec![
                TokenType::Add, TokenType::Sub, TokenType::VarRef, TokenType::Ref, TokenType::Deref
            ])),
            None => Err(ParseError::ExpectedToken)
        }
    }
    fn primary(&mut self) -> Result<Node, ParseError> {
        match self.peek_tk() {
            Some(tk @ Token { t: TokenType::Str(_), .. }) |
            Some(tk @ Token { t: TokenType::True, .. }) |
            Some(tk @ Token { t: TokenType::False, .. }) |
            Some(tk @ Token { t: TokenType::Real(_), .. }) |
            Some(tk @ Token { t: TokenType::Integer(_), .. }) |
            Some(tk @ Token { t: TokenType::Character(_), .. }) => {
                _ = self.next_tk(); // Discart prev Token
                let leaf = Box::new(ASTNode::Leaf(tk));
                Ok(Node::new(ExprType::from(&leaf), leaf))
            },
            Some(Token { t: TokenType::OpParen, .. }) => {
                let mut in_parenthesis = self.parse_with_delim_and_end_line(
                    Some(TokenType::Comma),
                    TokenType::OpParen,
                    TokenType::ClParen,
                    |s| s.expr()
                )?;
                if in_parenthesis.len() == 1 {
                    let mut node = Node::new(ExprType::None, Box::new(ASTNode::Empty));
                    std::mem::swap(&mut node, &mut in_parenthesis[0]);
                    Ok(node)
                } else {
                    Ok(Node::new(Unknown(self.get_unknown_id()), Box::new(ASTNode::Tuple(in_parenthesis))))
                }
            },
            Some(Token { t: TokenType::Id(_), .. }) |
            Some(Token { t: TokenType::StruAccess(_), .. }) |
            Some(Token { t: TokenType::ModAccess(_), .. }) => self.id(),
            Some(tk) => Err(ParseError::TokenNotExpected(tk, vec![
                            TokenType::Real(String::new()), TokenType::Integer(String::new()),
                            TokenType::Character(String::new()), TokenType::Str(String::new()),
                            TokenType::OpParen, TokenType::False, TokenType::True
                        ])),
            None => Err(ParseError::ExpectedToken)
        }
    }
    fn id(&mut self) -> Result<Node, ParseError> {
        let tk = self.assert_next(&[
            TokenType::Id(String::new()),
            TokenType::StruAccess(String::new()),
            TokenType::ModAccess(String::new()),
        ])?;
        let backup = self.get_state();
        let body = self.inner_body(Some(TokenType::Comma));
        Ok(Node::new(Unknown(self.get_unknown_id()),
            Box::new(if let Ok(_) = &body {
                ASTNode::StructInit(tk, body?)
            } else {
                self.set_state(&backup);
                ASTNode::Leaf(tk)
        })))
    }

    fn parse_compounded_type(
        &mut self,
        mut method: impl FnMut(&mut Self) -> Result<InnerNode, ParseError>,
        separator: TokenType
    ) -> Result<InnerNode, ParseError> {
        let first = method(self)?;
        if self.assert_peek(&[separator.clone()]).is_ok() {
            let mut v = vec![first];
            loop {
                match self.assert_peek(&[separator.clone()]) {
                    Ok(_) => {
                        _ = self.next_tk();
                        v.push(method(self)?)
                    }
                    Err(_) => break,
                }
            }
            Ok(Box::new(ASTNode::Type { t: separator, inner_types: v }))
        } else { Ok(first) }
    }

    fn _type_(&mut self) -> Result<Node, ParseError> {
        let t = self.parse_type()?;
        Ok(Node::new(ExprType::from(&t), Box::new(ASTNode::Empty)))
    }
    fn parse_type(&mut self) -> Result<InnerNode, ParseError> {
        self.parse_compounded_type(|s| s.union_type(), TokenType::FnType)
    }
    fn union_type(&mut self) -> Result<InnerNode, ParseError> {
        self.parse_compounded_type(|s| s.tuple_type(), TokenType::UnionType)
    }
    fn tuple_type(&mut self) -> Result<InnerNode, ParseError> {
        self.parse_compounded_type(|s| s.ptr_type(), TokenType::TupleType)
    }
    fn ptr_type(&mut self) -> Result<InnerNode, ParseError> {
        match self.peek_tk() {
            Some(Token { t: t @ TokenType::Ref, .. }) |
            Some(Token { t: t @ TokenType::VarRef, .. }) => {
                _ =  self.assert_next(&[TokenType::Ref, TokenType::VarRef])?;
                Ok(Box::new(ASTNode::Type { t, inner_types: vec![self.ptr_type()?] }))
            }
            _ => {
                self.basic_types()
            }
        }
    }

    fn basic_types(&mut self) -> Result<InnerNode, ParseError> {
        match self.peek_tk() {
            // Type inside parenthesis
            Some(Token { t: TokenType::OpParen, .. }) => {
                _ = self.assert_next(&[TokenType::OpParen])?;
                let t = self.parse_type();
                _ = self.assert_next(&[TokenType::ClParen])?;
                t
            },
            Some(Token { t: t @ TokenType::U(_), .. }) |
            Some(Token { t: t @ TokenType::I(_), .. }) |
            Some(Token { t: t @ TokenType::F(_), .. }) => {
                _ = self.next_tk();
                Ok(Box::new(ASTNode::Type { t, inner_types: vec![] }))
            },
            Some(_) => self.assert_next(&[
                            TokenType::Char,
                            TokenType::Bool,
                            TokenType::None,
                            TokenType::Type,
                            TokenType::Id(String::new()),
                        ]).map(|e| Box::new(ASTNode::Type { t: e.t, inner_types: vec![] })),
            _ => Err(ParseError::GeneralError(format!("{:?} is not a basic type.", self.peek_tk())))
        }
    }

}
