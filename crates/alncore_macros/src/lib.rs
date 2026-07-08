// filename: crates/alncore_macros/src/lib.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis

//! Procedural macros for compile-time ALN grammar validation.
//!
//! This crate is intentionally minimal. It provides `aln_grammar_guard`
//! to validate ecosafety-focused ALN snippets included as `&'static str`
//! literals in tests, examples, or docs. The parser here must mirror the
//! behavior of the runtime alncore parser, but is self-contained to avoid
//! cross-crate initialization during macro expansion.

#![forbid(unsafe_code)]

extern crate proc_macro;

use proc_macro::TokenStream;
use quote::quote;
use syn::{parse_macro_input, LitStr};

/// Compile-time ALN grammar guard for ecosafety snippets.
///
/// Usage:
/// ```rust
/// use alncore_macros::aln_grammar_guard;
///
/// #[aln_grammar_guard]
/// const ECOSAFETY_EXAMPLE: &str = r#"
/// record AlnDocument2026v1
///   docid EcoNet-Ecosafety-Core-2026
///   schemaname AlnDocument2026v1
///   versiontag v1
///   category ecosafety
///   roleband RESEARCH
///   ownerdid bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7
///   evidencehex 0xabc123
///   signinghex 0xdef456
/// endrecord
/// "#;
/// ```
#[proc_macro_attribute]
pub fn aln_grammar_guard(_attr: TokenStream, item: TokenStream) -> TokenStream {
    // We only support simple const string literals. Parse the incoming item first.
    let input = parse_macro_input!(item as syn::ItemConst);

    let lit = match &input.expr {
        syn::Expr::Lit(expr_lit) => match &expr_lit.lit {
            syn::Lit::Str(s) => s.clone(),
            _ => {
                return syn::Error::new_spanned(
                    &input.expr,
                    "aln_grammar_guard requires a string literal (&'static str)",
                )
                .to_compile_error()
                .into();
            }
        },
        _ => {
            return syn::Error::new_spanned(
                &input.expr,
                "aln_grammar_guard requires a string literal (&'static str)",
            )
            .to_compile_error()
            .into();
        }
    };

    // Perform minimal inline parsing and validation against the ecosafety ALN grammar.
    if let Err(err) = validate_ecosafety_aln_literal(&lit) {
        let msg = format!(
            "ALN grammar validation failed for ecosafety snippet: {}",
            err
        );
        return syn::Error::new_spanned(LitStr::new(&lit.value(), lit.span()), msg)
            .to_compile_error()
            .into();
    }

    // If validation passes, emit the original const unchanged.
    TokenStream::from(quote! { #input })
}

/// Minimal parse/validate for ecosafety-focused ALN snippets.
///
/// This is intentionally strict but shallow: we only check the presence and
/// basic shape of key ecosafety blocks and required fields. Deeper semantic
/// checks are delegated to runtime validation in the alncore crate.
fn validate_ecosafety_aln_literal(lit: &LitStr) -> Result<(), String> {
    let src = lit.value();
    let mut has_doc = false;
    let mut has_owner_did = false;
    let mut has_role_band = false;
    let mut has_evidence_hex = false;
    let mut has_signing_hex = false;

    for (idx, raw_line) in src.lines().enumerate() {
        let line = raw_line.trim();

        if line.is_empty() || line.starts_with('#') {
            continue;
        }

        if line.starts_with("record ") && line.contains("AlnDocument2026v1") {
            has_doc = true;
            continue;
        }

        if line.starts_with("ownerdid ") {
            has_owner_did = true;
            continue;
        }

        if line.starts_with("roleband ") {
            has_role_band = true;
            continue;
        }

        if line.starts_with("evidencehex ") {
            has_evidence_hex = true;
            continue;
        }

        if line.starts_with("signinghex ") {
            has_signing_hex = true;
            continue;
        }

        // Basic lexing: enforce key value pairs without embedded tabs.
        if !line.starts_with("record ")
            && !line.starts_with("endrecord")
            && !line.starts_with("section ")
            && !line.starts_with("endsection")
        {
            let parts: Vec<&str> = line.split_whitespace().collect();
            if parts.len() < 2 {
                return Err(format!(
                    "Line {}: expected 'key value' pair, found '{}'",
                    idx + 1,
                    raw_line
                ));
            }
        }
    }

    if !has_doc {
        return Err("missing 'record AlnDocument2026v1' block".to_string());
    }
    if !has_owner_did {
        return Err("missing required field 'ownerdid'".to_string());
    }
    if !has_role_band {
        return Err("missing required field 'roleband'".to_string());
    }
    if !has_evidence_hex {
        return Err("missing required field 'evidencehex'".to_string());
    }
    if !has_signing_hex {
        return Err("missing required field 'signinghex'".to_string());
    }

    Ok(())
}
