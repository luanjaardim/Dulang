use std::rc::Rc;

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum TokenType {
    Str, Integer, Real, Character, Comment, Id,

    // Numerical Operations
    Add, Sub, Mul, Div,                     // +, -. *, /
    // Numerical Comparisons
    Eq, Neq, GrT, GrE, LeT, LeE,            // ==, !=, >, >=, <, <=
    // Logical Operations
    And, Or, Not,                           // and, or, not
    // Bitwise Operations
    Band, Bor, Bnot, Shl, Shr, Bxor,        // band, bor, bnot, shl, shr, bxor
    // Blocks
    If, Elif, Else, While, Loop, Switch,    // if, elif, else, while, loop, switch
    // Control Keywords
    Skip, Stop, Back,                       // skip, stop, back

    // Types
    I32, U32, Char, F32, F64, Bool,

    // Compounded types
    FnType, UnionType, TupleType,           // ->, ^, &

    Assign, FnBar, FnReturn, TypeInf,       // =, |, =>, ::

    // Symbols
    OpCurly, ClCurly, OpParen, ClParen,    // {, }, (, ),
    Comma, Dot, Semicolon,                 // ',' , '.' , ';'
    Nothing,                               // '()',

    Nl,                                    // New line '\n'

    //RealAdd, RealSub, RealMul, RealDiv,   // +., -., *., /. (Maybe?)
}
use TokenType::*;

#[derive(Clone)]
pub struct Token {
    c : usize,
    l : usize,
    pub t : TokenType,
    pub text : String,
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
                "{" => OpCurly, "}" => ClCurly, "(" => OpParen,")" => ClParen, "," => Comma, "." => Dot, ";" => Semicolon,
                "=" => Assign, "=>" => FnReturn, "|" => FnBar, "::" => TypeInf, "()" => Nothing,
                "i32" => I32, "u32" => U32, "char" => Char, "f32" => F32, "f64" => F64, "bool" => Bool,
                "->" => FnType, "^" => UnionType, "&" => TupleType,
                "if" => If, "elif" => Elif, "else" => Else,
                "while" => While, "loop" => Loop,
                _ if regex::Regex::new(r"^[_a-zA-Z]+").unwrap().is_match(text) => TokenType::Id,
                _ => panic!("Token type is unkown: {text}")
            }),
            text: String::from(text)
        }

    }

    pub fn nl(next_line: usize) -> Token { Token { c: 0, l: next_line, t: Nl, text: "\\n".to_string() } }

    pub fn position(&self) -> (usize, usize) { (self.l, self.c) }
}

// this will omit the position
impl std::fmt::Debug for Token {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Token({:?}, {})", self.t, self.text)
    }
}

// this will show the position, the type and the text
// NOTE: The Debug do not show the position for a better ASTNode printing, so the Display will show
impl std::fmt::Display for Token {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Token({:?}, {}) at (lin: {}, col: {})", self.t, self.text, self.l, self.c)
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
    const SEPARATORS: [&'static str; 24] = [
      "=>", "==", ">=", "<=", "!=", "::", "->", "()", "|", "=", ",", ";", "+", "-", "*", "/", "^", "&", "(", ")", "{", "}", "[", "]"
    ];

    // NOTE: the order is important here, as every Real contains Integer it must goes first
    const PATTERNS: [(&'static str, TokenType); 5] = [
        (r#"^(\$[^\$\n]*\n|\$\$[^\$]*\$\$)"#, Comment),
        (r#"^(\".*\"|\'\'(\w|\W)*\'\')"#, Str), (r"^\'(.|\\[rnt])\'", Character),
        (r"^(\d+\.\d*|\.\d+|\d+e(-?)\d+)", Real), (r"^\d+", Integer),
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

        // Skip whitespaces
        self.skip_ascii_whitespaces();

        // Here we can search for some general pattern (Strings, Real Numbers, Comments, and return the Token early)
        if let Some((m, t)) = self.match_patterns() {
            let len = m.range().len();
            let text = self.s[self.pos..self.pos+len].to_string();
            self.pos += len;
            self.c += len;
            return match t {
                Comment => self.next(), // Continue search if it's a comment
                _ => {
                    self.read_tks.push(Token { c: self.c, l: self.l, t, text });
                    Some(self.get_cur_tk_and_advance())
                }
            }
        }

        let rest = &self.s[self.pos..];
        // Now we are looking only to the next word(the first chars that are not whitespaces)
        let word = match rest.chars().enumerate().find(|e| e.1.is_ascii_whitespace()) {
            Some((pos, _)) => &rest[0..pos],
            None => if rest.is_empty() { return None } else { rest }
        };

        let sep_and_pos = Tokenizer::SEPARATORS.iter()
                              .filter_map(|sep| word.find(*sep).map_or(None, |pos| Some((sep, pos))))
                              // at the first position we have the separator, and the second is its position
                              .reduce(|acc, cur|
                                      if cur.1 < acc.1 { cur }  // if the cur separator appeared before
                                      else { acc }
                              );
        let tk = match sep_and_pos {
            Some((sep, pos)) => {
                if pos == 0 {
                    // there is no Token before the separator
                    Token::new(self.c, self.l, sep)
                } else {
                    // will store the sep Token and the token before it
                    self.read_tks.push(Token::new(self.c, self.l, &word[0..pos]));
                    Token::new(self.c+pos, self.l, sep)
                }
            },
            None => Token::new(self.c, self.l, word)
        };
        // update counters positions
        let consumed_chars = if sep_and_pos.is_none() { word.len() } else { sep_and_pos?.1 + sep_and_pos?.0.len() };
        self.pos += consumed_chars;
        self.c += consumed_chars;

        self.read_tks.push(tk);
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

    fn match_patterns(&self) -> Option<(regex::Match, TokenType)> {
        let text = &self.s[self.pos..];
        let p = Tokenizer::PATTERNS.iter().find(|p|
            regex::Regex::new(p.0).unwrap().is_match(text)
        )?;
        Some((regex::Regex::new(p.0).unwrap().find(text)?, p.1))
    }

    fn skip_ascii_whitespaces(&mut self) {
        for c in self.s[self.pos..].chars() {
            if !c.is_ascii_whitespace() { break }
            if c == '\n' { self.l += 1; self.c = 0; } else { self.c += 1; }
            self.pos += 1;
        }
    }
}

impl Iterator for Tokenizer {
    type Item = Token;
    fn next(&mut self) -> Option<Self::Item> {
        self.next()
    }
}
