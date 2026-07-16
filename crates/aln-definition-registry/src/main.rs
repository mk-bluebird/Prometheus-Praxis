use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};
use walkdir::WalkDir;
use regex::Regex;

#[derive(Debug)]
struct ParticleEntry {
    particle_name: String;
    aln_file_path: String;
    aln_title: Option<String>;
    aln_version: Option<String>;
    network: Option<String>;
    shardid: Option<String>;
    owner_bostrom: Option<String>;
    timestamp_utc: Option<String>;
}

fn find_aln_files(root: &Path) -> Vec<PathBuf> {
    WalkDir::new(root)
        .into_iter()
        .filter_map(Result::ok)
        .filter(|e| e.path().extension().and_then(|e| e.to_str()) == Some("aln"))
        .map(|e| e.path().to_path_buf())
        .collect()
}

fn parse_header_fields(content: &str) -> (Option<String>, Option<String>, Option<String>, Option<String>) {
    let mut title = None;
    let mut version = None;
    let mut network = None;
    let mut shardid = None;

    for line in content.lines() {
        let line = line.trim();
        if line.starts_with("title ") {
            title = Some(line["title ".len()..].trim().to_string());
        } else if line.starts_with("version ") {
            version = Some(line["version ".len()..].trim().to_string());
        } else if line.starts_with("network ") {
            network = Some(line["network ".len()..].trim().to_string());
        } else if line.starts_with("shardid ") {
            shardid = Some(line["shardid ".len()..].trim().to_string());
        }
    }

    (title, version, network, shardid)
}

fn parse_owner_timestamp(content: &str) -> (Option<String>, Option<String>) {
    let mut owner = None;
    let mut ts = None;

    for line in content.lines() {
        let line = line.trim();
        if line.starts_with("owner.bostrom ") {
            owner = Some(line["owner.bostrom ".len()..].trim().to_string());
        } else if line.starts_with("timestamp.utc ") {
            ts = Some(line["timestamp.utc ".len()..].trim().to_string());
        }
    }

    (owner, ts)
}

fn extract_particles(path: &Path) -> Vec<ParticleEntry> {
    let content = match fs::read_to_string(path) {
        Ok(c) => c,
        Err(_) => return Vec::new(),
    };

    let (title, version, network, shardid) = parse_header_fields(&content);
    let (owner_bostrom, timestamp_utc) = parse_owner_timestamp(&content);

    let particle_re = Regex::new(r"(?m)^\s*particle\s+([A-Za-z0-9_]+)").unwrap();

    let mut entries = Vec::new();
    for cap in particle_re.captures_iter(&content) {
        let name = cap[1].to_string();
        entries.push(ParticleEntry {
            particle_name: name,
            aln_file_path: path.to_string_lossy().to_string(),
            aln_title: title.clone(),
            aln_version: version.clone(),
            network: network.clone(),
            shardid: shardid.clone(),
            owner_bostrom: owner_bostrom.clone(),
            timestamp_utc: timestamp_utc.clone(),
        });
    }

    entries
}

fn load_registry_template(path: &Path) -> String {
    fs::read_to_string(path).unwrap_or_else(|_| {
        panic!(
            "Failed to read registry template at {}",
            path.display()
        )
    })
}

fn write_registry(path: &Path, template: &str, entries: &[ParticleEntry]) {
    let mut output = String::new();
    let mut in_block = false;

    for line in template.lines() {
        let trimmed = line.trim();
        if trimmed.starts_with("block registry_rows") {
            in_block = true;
            output.push_str(line);
            output.push('\n');
            // skip until content/endcontent; we will inject new content
            continue;
        }

        if in_block && trimmed.starts_with("content") {
            // write content header and generated rows
            output.push_str("  content\n");
            output.push_str("    ; particle_name\taln_file_path\taln_title\taln_version\tnetwork\tshardid\towner_bostrom\ttimestamp_utc\n");
            for e in entries {
                let row = format!(
                    "    {pn}\t{path}\t{title}\t{ver}\t{net}\t{shard}\t{owner}\t{ts}\n",
                    pn = e.particle_name,
                    path = e.aln_file_path,
                    title = e.aln_title.clone().unwrap_or_default(),
                    ver = e.aln_version.clone().unwrap_or_default(),
                    net = e.network.clone().unwrap_or_default(),
                    shard = e.shardid.clone().unwrap_or_default(),
                    owner = e.owner_bostrom.clone().unwrap_or_default(),
                    ts = e.timestamp_utc.clone().unwrap_or_default()
                );
                output.push_str(&row);
            }
            output.push_str("  endcontent\n");
            // skip to endblock in original template
            in_block = false;
            continue;
        }

        if !in_block {
            output.push_str(line);
            output.push('\n');
        }
    }

    let mut file = fs::File::create(path).expect("Failed to write registry file");
    file.write_all(output.as_bytes())
        .expect("Failed to write registry content");
}

fn main() {
    let repo_root = Path::new(".");
    let aln_root = repo_root.join("aln");

    let aln_files = find_aln_files(&aln_root);

    let mut entries = Vec::new();
    for path in aln_files {
        let mut ps = extract_particles(&path);
        entries.append(&mut ps);
    }

    // Sort entries for stability.
    entries.sort_by(|a, b| {
        a.particle_name
            .cmp(&b.particle_name)
            .then(a.aln_file_path.cmp(&b.aln_file_path))
    });

    let registry_path = aln_root.join("aln_particle_registry.aln");
    let template = load_registry_template(&registry_path);

    write_registry(&registry_path, &template, &entries);
}
