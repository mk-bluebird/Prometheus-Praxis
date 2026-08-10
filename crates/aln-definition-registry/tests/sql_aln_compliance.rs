// File: crates/aln-definition-registry/tests/sql_aln_compliance.rs
use std::path::Path;

#[test]
fn registered_aln_corridors_are_enforced_by_their_bound_sql_schemas() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR"))
        .ancestors()
        .nth(2)
        .expect("crate is located under crates/")
        .to_path_buf();

    aln_definition_registry::verify_repository(&root)
        .expect("every ALN invariant must appear in its bound SQL CHECK or trigger condition");
}
