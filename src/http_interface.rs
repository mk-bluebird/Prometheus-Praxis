// filename src/http_interface.rs
// destination eco_restoration_shard/src/http_interface.rs

use std::net::SocketAddr;

use serde::{Deserialize, Serialize};
use serde_json::json;
use tiny_http::{Response, Server};

use crate::ecoconstellation_index::EcoConstellationIndex;

#[derive(Debug, Serialize, Deserialize)]
pub struct ResolveRepoRequest {
    pub githubslug: String,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ResolveRepoResponse {
    pub repoid: i64,
    pub name: String,
    pub githubslug: String,
    pub visibility: String,
    pub roleband: String,
}

pub fn run_http_interface(addr: SocketAddr, db_path: &str) -> std::io::Result<()> {
    let server = Server::http(addr)?;
    let conn = rusqlite::Connection::open(db_path).expect("open DB");
    let index = EcoConstellationIndex::new(conn);

    for request in server.incoming_requests() {
        let url = request.url().to_string();

        if url == "/health" {
            let response = Response::from_string("ok");
            request.respond(response)?;
            continue;
        }

        if url == "/resolve_repo" && *request.method() == tiny_http::Method::Post {
            let mut body = String::new();
            request.as_reader().read_to_string(&mut body)?;
            let req: ResolveRepoRequest =
                serde_json::from_str(&body).unwrap_or(ResolveRepoRequest {
                    githubslug: String::new(),
                });

            let mut stmt = index
                .conn
                .prepare(
                    "SELECT repoid, name, githubslug, visibility, roleband
                     FROM repo
                     WHERE githubslug = ?1",
                )
                .unwrap();

            let mut rows = stmt.query(params![req.githubslug]).unwrap();
            if let Some(row) = rows.next().unwrap() {
                let resp = ResolveRepoResponse {
                    repoid: row.get(0).unwrap(),
                    name: row.get(1).unwrap(),
                    githubslug: row.get(2).unwrap(),
                    visibility: row.get(3).unwrap(),
                    roleband: row.get(4).unwrap(),
                };
                let json_body = serde_json::to_string(&resp).unwrap();
                let response = Response::from_string(json_body)
                    .with_header(tiny_http::Header::from_bytes(&b"Content-Type"[..], &b"application/json"[..]).unwrap());
                request.respond(response)?;
            } else {
                let response = Response::from_string(
                    json!({"error": "repo not found"}).to_string(),
                )
                .with_status_code(404);
                request.respond(response)?;
            }

            continue;
        }

        let response = Response::from_string("not found").with_status_code(404);
        request.respond(response)?;
    }

    Ok(())
}
