// 41. T03 split-rule DSL parsing (core skeleton)
// filename: eco_wealth/src/split_dsl.rs
// destination: eco_wealth/src/split_dsl.rs

// Non-actuating parser/evaluator for EcoWealthSplitRule2026v1 expressions.
// K/E/R inputs must already be computed by the ecosafety core.[file:7][file:11]

use core::str::Chars;
use alloc::vec::Vec;
use alloc::string::{String, ToString};
use alloc::collections::BTreeMap;

#[derive(Debug, Clone, PartialEq)]
pub enum Expr {
    Number(f64),
    Ident(String),
    Add(Box<Expr>, Box<Expr>),
    Sub(Box<Expr>, Box<Expr>),
    Mul(Box<Expr>, Box<Expr>),
    Div(Box<Expr>, Box<Expr>),
    Max(Box<Expr>, Box<Expr>),
    Min(Box<Expr>, Box<Expr>),
}

#[derive(Debug)]
pub enum DslError {
    UnexpectedChar(char),
    UnexpectedEof,
    InvalidIdent(String),
    DivisionByZero,
    MissingSumShare,
}

pub type Env = BTreeMap<String, f64>;

pub struct Parser<'a> {
    src: Chars<'a>,
    look: Option<char>,
}

impl<'a> Parser<'a> {
    pub fn new(s: &'a str) -> Self {
        let mut it = s.chars();
        let look = it.next();
        Parser { src: it, look }
    }

    fn bump(&mut self) {
        self.look = self.src.next();
    }

    fn skip_ws(&mut self) {
        while matches!(self.look, Some(c) if c.is_ascii_whitespace()) {
            self.bump();
        }
    }

    pub fn parse_expr(&mut self) -> Result<Expr, DslError> {
        let e = self.parse_term()?;
        self.parse_expr_tail(e)
    }

    fn parse_expr_tail(&mut self, mut left: Expr) -> Result<Expr, DslError> {
        loop {
            self.skip_ws();
            match self.look {
                Some('+') => {
                    self.bump();
                    let right = self.parse_term()?;
                    left = Expr::Add(Box::new(left), Box::new(right));
                }
                Some('-') => {
                    self.bump();
                    let right = self.parse_term()?;
                    left = Expr::Sub(Box::new(left), Box::new(right));
                }
                _ => return Ok(left),
            }
        }
    }

    fn parse_term(&mut self) -> Result<Expr, DslError> {
        let mut left = self.parse_factor()?;
        loop {
            self.skip_ws();
            match self.look {
                Some('*') => {
                    self.bump();
                    let right = self.parse_factor()?;
                    left = Expr::Mul(Box::new(left), Box::new(right));
                }
                Some('/') => {
                    self.bump();
                    let right = self.parse_factor()?;
                    left = Expr::Div(Box::new(left), Box::new(right));
                }
                _ => return Ok(left),
            }
        }
    }

    fn parse_factor(&mut self) -> Result<Expr, DslError> {
        self.skip_ws();
        match self.look {
            Some(c) if c.is_ascii_digit() || c == '.' => self.parse_number(),
            Some(c) if c.is_ascii_alphabetic() => self.parse_ident_or_fn(),
            Some('(') => {
                self.bump();
                let e = self.parse_expr()?;
                self.skip_ws();
                match self.look {
                    Some(')') => {
                        self.bump();
                        Ok(e)
                    }
                    Some(c) => Err(DslError::UnexpectedChar(c)),
                    None => Err(DslError::UnexpectedEof),
                }
            }
            Some(c) => Err(DslError::UnexpectedChar(c)),
            None => Err(DslError::UnexpectedEof),
        }
    }

    fn parse_number(&mut self) -> Result<Expr, DslError> {
        let mut s = String::new();
        while matches!(self.look, Some(c) if c.is_ascii_digit() || c == '.') {
            s.push(self.look.unwrap());
            self.bump();
        }
        let v: f64 = s.parse().unwrap_or(0.0);
        Ok(Expr::Number(v))
    }

    fn parse_ident_or_fn(&mut self) -> Result<Expr, DslError> {
        let mut s = String::new();
        while matches!(self.look, Some(c) if c.is_ascii_alphanumeric() || c == '_' ) {
            s.push(self.look.unwrap());
            self.bump();
        }
        self.skip_ws();
        // Recognize max/min functions
        if s == "max" || s == "min" {
            if self.look != Some('(') {
                return Err(DslError::UnexpectedChar(self.look.unwrap_or(' ')));
            }
            self.bump();
            let a = self.parse_expr()?;
            self.skip_ws();
            if self.look != Some(',') {
                return Err(DslError::UnexpectedChar(self.look.unwrap_or(' ')));
            }
            self.bump();
            let b = self.parse_expr()?;
            self.skip_ws();
            if self.look != Some(')') {
                return Err(DslError::UnexpectedChar(self.look.unwrap_or(' ')));
            }
            self.bump();
            return Ok(if s == "max" {
                Expr::Max(Box::new(a), Box::new(b))
            } else {
                Expr::Min(Box::new(a), Box::new(b))
            });
        }
        Ok(Expr::Ident(s))
    }
}

pub fn eval_expr(expr: &Expr, env: &Env) -> Result<f64, DslError> {
    match expr {
        Expr::Number(v) => Ok(*v),
        Expr::Ident(name) => env
            .get(name)
            .copied()
            .ok_or_else(|| DslError::InvalidIdent(name.clone())),
        Expr::Add(a, b) => Ok(eval_expr(a, env)? + eval_expr(b, env)?),
        Expr::Sub(a, b) => Ok(eval_expr(a, env)? - eval_expr(b, env)?),
        Expr::Mul(a, b) => Ok(eval_expr(a, env)? * eval_expr(b, env)?),
        Expr::Div(a, b) => {
            let denom = eval_expr(b, env)?;
            if denom == 0.0 {
                Err(DslError::DivisionByZero)
            } else {
                Ok(eval_expr(a, env)? / denom)
            }
        }
        Expr::Max(a, b) => Ok(eval_expr(a, env)?.max(eval_expr(b, env)?)),
        Expr::Min(a, b) => Ok(eval_expr(a, env)?.min(eval_expr(b, env)?)),
    }
}

// Example: steward-level split
//
// share_expr    = "W_i * K_i * E_i"
// normalize_expr= "" (implicit Σ share normalization)
// forbid_expr   = "R_i > RISK_CAP"
//
// T03 supplies per-steward env:
//   TOTAL_EW, W_i, K_i, E_i, R_i, RISK_CAP, SUM_share (for normalize_expr).
// This evaluator is non-actuating and only returns scalars.[file:7][file:11]
