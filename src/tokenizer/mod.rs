use std::rc::Rc;
pub mod tokentype_partialeq;

use TokenType::*;
#[derive(Debug, Clone)]
pub enum TokenType {
    Str(String), Integer(String), Real(String), Character(String), Comment, Id(String),

    // Numerical Operations
    Add, Sub, Mul, Div,                     // +, -. *, /
    // Numerical Comparisons
    Eq, Neq, GrT, GrE, LeT, LeE,            // ==, !=, >, >=, <, <=
    // Logical Operations
    And, Or, Not,                           // and, or, not
    // Bitwise Operations
    Band, Bor, Bnot, Shl, Shr, Bxor,        // band, bor, bnot, shl, shr, bxor
    // Blocks
    Mod, If, Elif, Else, While, Loop,       // mod, if, elif, else, while, loop
    // Control Keywords
    Skip, Stop, Back,                       // skip, stop, back
    // False and True keywords
    False, True,                            // false, true

    // Types
    I(usize), U(usize), F(usize), Char, Bool,

    // Compounded types
    FnType, UnionType, TupleType,           // ->, |, ^

    Assign, Type, TypeInf,                  // =, type, ::

    VarDef, Ref, VarRef, Deref,             // var, &, &var, @

    // Symbols
    OpCurly, ClCurly, OpParen, ClParen,     // {, }, (, ),
    Comma, Dot, Semicolon, Colon,           // ',' , '.' , ';', ':'
    PassR, PassL,                           // >>, <<
    None,                                   // none,

    Nl,                                     // New line '\n'

    //RealAdd, RealSub, RealMul, RealDiv,   // +., -., *., /. (Maybe?)
}

#[derive(Clone)]
pub struct Token {
    c : usize,
    l : usize,
    pub t : TokenType,
}

impl Token {
    pub fn new(c: usize, l: usize, text: &str) -> Self {
        Token {
            c, l,
            t: (match text {
                "+" => Add, "-" => Sub, "*" => Mul, "/" => Div,
                "==" => Eq, "!=" => Neq, ">" => GrT, ">=" => GrE, "<" => LeT, "<=" => LeE,
                "and" => And, "or" => Or, "not" => Not,
                "band" => Band, "bor" => Bor, "bnot" => Bnot, "bxor" => Bxor, "shl" => Shl, "shr" => Shr,
                "{" => OpCurly, "}" => ClCurly, "(" => OpParen,")" => ClParen, "," => Comma, ";" => Semicolon, ":" => Colon,
                "=" => Assign, "::" => TypeInf, "none" => TokenType::None, "char" => Char, "bool" => Bool,
                "->" => FnType, "|" => UnionType, "^" => TupleType, "type" => Type,
                "&var" => VarRef, "var" => VarDef, "&" => Ref, "@" => Deref,
                ">>" => PassR, "<<" => PassL,
                "if" => If, "elif" => Elif, "else" => Else,
                "while" => While, "loop" => Loop, "mod" => Mod,
                "skip" => Skip, "stop" => Stop, "back" => Back,
                "false" => False, "true" => True,
                _ if regex::Regex::new(r"^i\d+$").unwrap().is_match(text) => I(text[1..].parse().unwrap()),
                _ if regex::Regex::new(r"^u\d+$").unwrap().is_match(text) => U(text[1..].parse().unwrap()),
                _ if regex::Regex::new(r"^f\d+$").unwrap().is_match(text) => F(text[1..].parse().unwrap()),
                _ if regex::Regex::new(r#"^(\".*\"|\'\'(\w|\W)*\'\')"#).unwrap().is_match(text) => TokenType::Str(text.to_string()),
                _ if regex::Regex::new(r#"^\w+(\.(\d|\w)+)+"#).unwrap().is_match(text) => TokenType::StruAccess(text.to_string()),
                _ if regex::Regex::new(r#"^\w+(:\w+)+"#).unwrap().is_match(text) => TokenType::ModAccess(text.to_string()),
                _ if regex::Regex::new(r"^\'(.|\\[rnt])\'").unwrap().is_match(text) => TokenType::Character(text.to_string()),
                _ if regex::Regex::new(r"^(\d+\.\d*|\.\d+|\d+e(-?)\d+)").unwrap().is_match(text) => TokenType::Real(text.to_string()),
                _ if regex::Regex::new(r"^\d+").unwrap().is_match(text) => Integer(text.to_string()),
                _ if regex::Regex::new(r"^\w(_|\w|\d)*$").unwrap().is_match(text) => TokenType::Id(text.to_string()),


                _ => panic!("Token type is unkown: {text} with len {}", text.len())
            }),
        }
    }

    pub fn new_with_tk_type(c: usize, l: usize, t: TokenType) -> Self { Token { c, l, t } }

    pub fn sep_type(text: &str) -> Option<TokenType> {
        Some(match text {
            "+" => Add, "-" => Sub, "*" => Mul, "/" => Div,
            "==" => Eq, "!=" => Neq, ">" => GrT, ">=" => GrE, "<" => LeT, "<=" => LeE,
            "{" => OpCurly, "}" => ClCurly, "(" => OpParen,")" => ClParen,
            "," => Comma, ";" => Semicolon, ":" => Colon,
            "=" => Assign, "::" => TypeInf, "->" => FnType, "|" => UnionType, "^" => TupleType,
            ">>" => PassR, "<<" => PassL, "&" => Ref, "@" => Deref,
            _ => return Option::None
        })
    }

    pub fn nl(next_line: usize) -> Token { Token { c: 0, l: next_line, t: Nl } }

    pub fn position(&self) -> (usize, usize) { (self.l, self.c) }
}

// this will omit the position
impl std::fmt::Debug for Token {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Token({:?})", self.t)
    }
}

// this will show the position, the type and the text
// NOTE: The Debug do not show the position for a better ASTNode printing, so the Display will show
impl std::fmt::Display for Token {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Token({:?}) at (lin: {}, col: {})", self.t, self.l, self.c)
    }
}

#[derive(Clone)]
pub struct Tokenizer {
    s: Rc<String>,
    pos: usize,          //index in s
    l: usize,            //count the number of '\n'
    c: usize,            //reset with '\n'
    tk_pos: usize,       //index in read_tks
    //used to store previous tokens, so it's not need to always recalculate them
    read_tks: Vec<Token>,
}

impl Tokenizer {

    // NOTE: the order is important here, put the separators with length 2 first
    const SEPARATORS: [&'static str; 26] = [
      "=>", "==", ">=", "<=", "!=", "::", "->", "^", "|", "@", "&var", "&", ":", "=", ",", ";", "+", "-", "*", "/", "(", ")", "{", "}", "[", "]"
    ];

    pub fn new(content_to_tokenize: Rc<String>) -> Self {
        Tokenizer {
            s: content_to_tokenize, pos: 0, tk_pos: 0, l: 0, c: 0, read_tks: vec![]
        }
    }

    /// Return the current state of the Tokenizer cursor:
    ///     tk_pos -> The position of the current Token to be read
    pub fn get_state(&self) -> usize { self.tk_pos }

    /// Set the state of the cursor, useful to store and go back to a previous position in the file
    /// Receives a position to the already read tokens
    pub fn set_state(&mut self, state: usize) { self.tk_pos = state; }

    fn get_cur_tk_and_advance(&mut self) -> Token {
        let tk = self.read_tks[self.tk_pos].clone();
        self.tk_pos += 1;
        tk
    }

    pub fn next(&mut self) -> Option<Token> {
        // Check if the Token in the cursor was already calculated
        if self.tk_pos < self.read_tks.len() { return Some(self.get_cur_tk_and_advance()) }

        #[derive(PartialEq)]
        enum State {
            Unknown,
            Separator,
            Generic,
            GenericWithColon,
            GenericWithDot,
            SingleLineComment,
            MultLineComment,
        }

        use State::*;
        let mut chars = self.s[self.pos..].chars();
        let (mut begin_ind, beg_c, beg_l) = (0, 0, 0);
        let mut state = Unknown;

        loop {
            if self.pos >= self.s.len() { return Option::None }
            match chars.next().unwrap() {
                // '$' => {
                //     state = SingleLineComment
                // },
                _ if state == SingleLineComment || state == MultLineComment => (),
                '+' | '-' | '/' | '*' | '=' | '>' | '<' | '!' => {
                    if state == Unknown { begin_ind = self.pos }
                    else if state == Separator {
                        let t = Token::sep_type(&self.s[begin_ind..=self.pos]);
                        if t.is_some() {
                            self.read_tks.push(Token::new_with_tk_type(beg_c, beg_l, t.unwrap()));
                            self.pos += 1;
                        } else {
                            self.read_tks.extend([
                              Token::new(beg_c, beg_l, &self.s[begin_ind..begin_ind+1]),
                              Token::new(beg_c+1, beg_l, &self.s[self.pos..self.pos+1])
                            ]);
                            self.pos += 2;
                        }
                        state = Unknown;
                        break
                    } else {
                        self.read_tks.push(Token::new(beg_c, beg_l, &self.s[begin_ind..self.pos]));
                        break
                    }

                    state = Separator;
                },
                '(' | ')' |'{' | '}'| '@' | '&' | '^' | '?' | ',' | ';' => {
                    if state == Unknown {
                        self.read_tks.push(Token::new(beg_c, beg_l, &self.s[self.pos..self.pos+1]));
                        self.pos += 1;
                        break
                    }
                },
                c if c.is_whitespace() => {
                    if state == Unknown {
                        ()
                    } else {
                        self.read_tks.push(Token::new(beg_c, beg_l, &self.s[begin_ind..self.pos]));
                        self.pos += 1;
                        state = Unknown;
                        break
                    }
                },
                '.' => {
                    if state == Unknown { begin_ind = self.pos }
                    if state == Generic || state == Unknown { state = GenericWithDot }
                },
                ':' => {
                    if state == Generic { state = GenericWithColon }
                    else if state != GenericWithColon {
                        if let Some(':') = chars.nth(self.pos+1) {
                            self.read_tks.extend([
                                Token::new(beg_c, beg_l, &self.s[begin_ind..self.pos]),
                                Token::new_with_tk_type(beg_c, beg_l, TokenType::TypeInf)
                            ]);
                            self.pos += 2;
                        } else {
                            self.read_tks.extend([
                                Token::new(beg_c, beg_l, &self.s[begin_ind..self.pos]),
                                Token::new_with_tk_type(beg_c, beg_l, TokenType::Colon)
                            ]);
                            self.pos +=1;
                        }
                        break
                    }
                }
                c if c.is_alphanumeric() => {
                    if state == Unknown {
                        begin_ind = self.pos;
                        state = Generic;
                    }
                },
                _ => unreachable!(),
            }
            self.pos += 1;
        }
        Some(self.get_cur_tk_and_advance())
    }

    pub fn peek(&mut self) -> Option<Token> {
        if let Some(tk) = self.read_tks.get(self.tk_pos) {
            return Some(tk.clone())
        }
        let cur_state = self.get_state();
        let tk = self.next();
        self.set_state(cur_state);
        tk
    }

}

impl Iterator for Tokenizer {
    type Item = Token;
    fn next(&mut self) -> Option<Self::Item> {
        self.next()
    }
}
