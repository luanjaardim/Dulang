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

    StruAccess(String), ModAccess(String),  // foo.bar.tar , foo:bar:tar

    // Symbols
    OpCurly, ClCurly, OpParen, ClParen,     // {, }, (, ),
    OpSqrBra, ClSqrBra,                     // [, ],
    Comma, Semicolon, Colon,                // ',' , ';', ':'
    PassR, PassL,                           // >>, <<
    None, PassDef, Extern,                  // none, pass, extern

    Nl,                                     // New line '\n'

    //RealAdd, RealSub, RealMul, RealDiv,   // +., -., *., /. (Maybe?)
}
impl TokenType {
    pub fn get_id_name(&self) -> Option<&str> {
        match self {
            Id(name) | StruAccess(name) | ModAccess(name) => Some(name),
            _ => Option::None,
        }
    }

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
                "{" => OpCurly, "}" => ClCurly, "(" => OpParen,")" => ClParen, "[" => OpSqrBra, "]" => ClSqrBra,
                "," => Comma, ";" => Semicolon, ":" => Colon,
                "=" => Assign, "::" => TypeInf, "none" => TokenType::None, "char" => Char, "bool" => Bool,
                "->" => FnType, "|" => UnionType, "^" => TupleType, "type" => Type,
                "&var" => VarRef, "var" => VarDef, "&" => Ref, "@" => Deref,
                ">>" => PassR, "<<" => PassL,
                "if" => If, "elif" => Elif, "else" => Else, "pass" => PassDef, "extern" => Extern,
                "while" => While, "loop" => Loop, "mod" => Mod,
                "skip" => Skip, "stop" => Stop, "back" => Back,
                "false" => False, "true" => True,
                _ if regex::Regex::new(r"^i\d+$").unwrap().is_match(text) => I(text[1..].parse().unwrap()),
                _ if regex::Regex::new(r"^u\d+$").unwrap().is_match(text) => U(text[1..].parse().unwrap()),
                _ if regex::Regex::new(r"^f\d+$").unwrap().is_match(text) => F(text[1..].parse().unwrap()),
                _ if regex::Regex::new(r#"^(\".*\"|\'\'(\w|\W)*\'\')$"#).unwrap().is_match(text) => TokenType::Str(text[1..text.len()-1].to_string()),
                _ if regex::Regex::new(r"^\'(.|\\[rnt])\'$").unwrap().is_match(text) => TokenType::Character(text.to_string()),
                _ if regex::Regex::new(r"^(\d+\.\d*|\.\d+|\d+e(-?)\d+)$").unwrap().is_match(text) => TokenType::Real(text.to_string()),
                _ if regex::Regex::new(r"^\d+$").unwrap().is_match(text) => Integer(text.to_string()),
                _ if regex::Regex::new(r#"^[_A-Za-z]\w*(\.\w+)+$"#).unwrap().is_match(text) => TokenType::StruAccess(text.to_string()),
                _ if regex::Regex::new(r#"^[_A-Za-z]\w*(:[_A-Za-z]\w*)+$"#).unwrap().is_match(text) => TokenType::ModAccess(text.to_string()),
                _ if regex::Regex::new(r"^[_A-Za-z]\w*$").unwrap().is_match(text) => TokenType::Id(text.to_string()),


                _ => panic!("Token type is unkown: {text}")
            }),
        }
    }

    pub fn new_with_tk_type(c: usize, l: usize, t: TokenType) -> Self { Token { c, l, t } }

    pub fn sep_type(text: &str) -> Option<TokenType> {
        Some(match text {
            "+" => Add, "-" => Sub, "*" => Mul, "/" => Div,
            "==" => Eq, "!=" => Neq, ">" => GrT, ">=" => GrE, "<" => LeT, "<=" => LeE,
            "{" => OpCurly, "}" => ClCurly, "(" => OpParen, ")" => ClParen, "[" => OpSqrBra, "]" => ClSqrBra,
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

fn is_alphanum_or_underscoe(c: char) -> bool {
    c.is_alphanumeric() || c == '_'
}

impl Tokenizer {

    // NOTE: the order is important here, put the separators with length 2 first
    const SEPARATORS: [&'static str; 26] = [
      "=>", "==", ">=", "<=", "!=", "::", "->", "^", "|", "@", "&var", "&", ":", "=", ",", ";", "+", "-", "*", "/", "(", ")", "{", "}", "[", "]"
    ];

    pub fn new(content_to_tokenize: Rc<String>) -> Self {
        Tokenizer {
            s: content_to_tokenize, pos: 0, tk_pos: 0, l: 1, c: 1, read_tks: vec![]
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

        #[derive(PartialEq, Debug)]
        enum State {
            Unknown,
            Separator,
            Generic, Numeric,
            String, Char,
            GenericWithColon,
            GenericWithDot,
            FoundComment, SingleLineComment, MultLineComment,
        }

        use State::*;
        let mut chars = self.s[self.pos..].chars();
        let (mut begin_ind, mut beg_c, mut beg_l) = (self.pos, self.c, self.l);
        let mut state = Unknown;

        let offset: usize = loop {
            let c = chars.next()?;
            if c == '\n' {
                if state == SingleLineComment { state = Unknown; }
                self.c = 0;
                self.l += 1;
            }
            match c {
                // Comments detection
                c if state == FoundComment || c == '$' => {
                    if state == FoundComment {
                        state = if c == '$' { MultLineComment } else { SingleLineComment };
                    } else {
                        // Searching for the end of a MultiLineComment
                        if state == MultLineComment && self.pos+2 < self.s.len() && &self.s[self.pos..self.pos+2] == "$$" {
                            _ = chars.next()?;
                            self.pos += 1;
                            self.c += 1;
                            state = Unknown;
                        } else if state != SingleLineComment && state != MultLineComment {
                            if state != Unknown {
                                self.read_tks.push(Token::new(beg_c, beg_l, &self.s[begin_ind..self.pos]));
                            }
                            state = FoundComment;
                        }
                    }
                },
                // Discarting elements if inside a comment
                _ if state == SingleLineComment || state == MultLineComment => (),
                '\\' if state == Char || state == String => {
                    _ = chars.next()?;
                    self.pos += 1;
                    self.c += 1;
                },
                '\"' | '\'' => {
                    if (state == Char && c == '\'') || (state == String && c == '\"') {
                        self.read_tks.push(Token::new(beg_c, beg_l, &self.s[begin_ind..=self.pos]));
                        break 1
                    }
                    if state != Unknown {
                        self.read_tks.push(Token::new(beg_c, beg_l, &self.s[begin_ind..self.pos]));
                    }
                    state = if c == '\'' { Char } else { String };
                    beg_c = self.c;
                    beg_l = self.l;
                    begin_ind = self.pos;
                },
                _ if state == Char || state == String => (),
                '+' | '-' | '/' | '*' | '=' | '>' | '<' | '!' => {
                    if state == Unknown {
                        beg_c = self.c;
                        beg_l = self.l;
                        begin_ind = self.pos;
                        state = Separator;
                    }
                    else if state == Separator {
                        let t = Token::sep_type(&self.s[begin_ind..=self.pos]);
                        if t.is_some() {
                            self.read_tks.push(Token::new_with_tk_type(beg_c, beg_l, t.unwrap()));
                            break 1
                        } else {
                            self.read_tks.extend([
                              Token::new(beg_c, beg_l, &self.s[begin_ind..begin_ind+1]),
                              Token::new(beg_c+1, beg_l, &self.s[self.pos..self.pos+1])
                            ]);
                            break 2
                        }
                    } else {
                        self.read_tks.push(Token::new(beg_c, beg_l, &self.s[begin_ind..self.pos]));
                        break 0
                    }
                },
                '(' | ')' |'{' | '}' | '[' | ']' | '@' | '&' | '|' | '^' | '?' | ',' | ';' => {
                    let (offset, t) = if self.pos+4 < self.s.len() && &self.s[self.pos..self.pos+4] == "&var" {
                        (4, TokenType::VarRef)
                    } else { (1, Token::sep_type(&self.s[self.pos..self.pos+1]).unwrap()) };

                    if state == Unknown {
                        self.read_tks.push(Token::new_with_tk_type(self.c, self.l, t));
                    } else {
                        self.read_tks.extend([
                            Token::new(beg_c, beg_l, &self.s[begin_ind..self.pos]),
                            Token::new_with_tk_type(self.c, self.l, t)
                        ]);
                    }
                    break offset
                },
                c if c.is_whitespace() => {
                    if state == Unknown {
                        ()
                    } else {
                        self.read_tks.push(Token::new(beg_c, beg_l, &self.s[begin_ind..self.pos]));
                        break 1
                    }
                },
                _ if state == Separator => {
                    self.read_tks.push(Token::new(beg_c, beg_l, &self.s[begin_ind..self.pos]));
                    break 0
                }
                '.' => {
                    if state == Unknown {
                        begin_ind = self.pos;
                        beg_c = self.c;
                        beg_l = self.l;
                    }
                    if state == Generic { state = GenericWithDot }
                    else if state == Unknown { state = Numeric } // Will be a Real that starts with '.': .2e2
                },
                ':' => {
                    match chars.clone().next() {
                        Some(':') => {
                            if state == Unknown {
                                self.read_tks.push(Token::new_with_tk_type(self.c, self.l, TokenType::TypeInf));
                            } else {
                                self.read_tks.extend([
                                    Token::new(beg_c, beg_l, &self.s[begin_ind..self.pos]),
                                    Token::new_with_tk_type(self.c, self.l, TokenType::TypeInf)
                                ]);
                            }
                            break 2
                        },
                        e if (state == Generic || state == GenericWithColon) && is_alphanum_or_underscoe(e.unwrap_or(' ')) => state = GenericWithColon,
                        _ => {
                            if state != Unknown {
                                self.read_tks.push(Token::new(beg_c, beg_l, &self.s[begin_ind..self.pos]));
                            }
                            self.read_tks.push(Token::new_with_tk_type(self.c, self.l, TokenType::Colon));
                            break 1
                        }
                    };
                },
                c if is_alphanum_or_underscoe(c) => {
                    if state == Unknown {
                        begin_ind = self.pos;
                        beg_c = self.c;
                        beg_l = self.l;
                        state = if c.is_alphabetic() { Generic } else { Numeric };
                    }
                },
                c => unreachable!("No match arm handle: {c}."),
            }
            self.pos += 1;
            self.c += 1;
        };
        self.pos += offset;
        self.c += offset;
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
