// filename: src/tools/spine_phoenix_governance_probe.rs
// destination: Eco-Fort/src/tools/spine_phoenix_governance_probe.rs
// repo-target: github.com/mk-bluebird/eco_restoration_shard

use std::env;
use std::process;

use rusqlite::Connection;

#[derive(Debug)]
struct Options {
    db_path: String,
    show_definitions: bool,
    show_restoration_views: bool,
    show_ecoperjoule_views: bool,
    show_mt6883_views: bool,
}

fn parse_args() -> Options {
    let mut db_path = String::from("db/restorationindex.sqlite3");
    let mut show_definitions = false;
    let mut show_restoration_views = false;
    let mut show_ecoperjoule_views = false;
    let mut show_mt6883_views = false;

    let args: Vec<String> = env::args().collect();
    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "--db" => {
                if i + 1 >= args.len() {
                    eprintln!("--db requires a path argument");
                    process::exit(1);
                }
                db_path = args[i + 1].clone();
                i += 1;
            }
            "--definitions" => {
                show_definitions = true;
            }
            "--restoration-phx" => {
                show_restoration_views = true;
            }
            "--ecoperjoule-phx" => {
                show_ecoperjoule_views = true;
            }
            "--mt6883-phx" => {
                show_mt6883_views = true;
            }
            other => {
                eprintln!("Unknown argument: {}", other);
                process::exit(1);
            }
        }
        i += 1;
    }

    Options {
        db_path,
        show_definitions,
        show_restoration_views,
        show_ecoperjoule_views,
        show_mt6883_views,
    }
}

fn open_readonly(path: &str) -> Connection {
    Connection::open_with_flags(path, rusqlite::OpenFlags::SQLITE_OPEN_READ_ONLY)
        .unwrap_or_else(|e| {
            eprintln!("Failed to open DB {}: {}", path, e);
            process::exit(1);
        })
}

fn print_sep() {
    println!("----------------------------------------------------------------");
}

fn show_definitionregistry_phx(conn: &Connection) {
    let sql = r#"
        SELECT defid,
               logicalname,
               versiontag,
               repo,
               filepath,
               category,
               description,
               frozen
        FROM definitionregistry
        WHERE logicalname IN (
            'restoration.blastradius.phoenix',
            'energy.ecoperjoule.policy.phoenix',
            'mt6883.lane.continuity.phoenix',
            'restoration.identity.binding.phoenix',
            'restoration.energy.mt6883.tool.phoenix'
        )
        ORDER BY logicalname, versiontag;
    "#;

    let mut stmt = conn
        .prepare(sql)
        .expect("prepare definitionregistry query failed");

    let mut rows = stmt
        .query([])
        .expect("query definitionregistry failed");

    println!("Phoenix-AZ DefinitionRegistry entries (restoration / energy / MT6883 / identity):");
    print_sep();

    while let Ok(Some(row)) = rows.next() {
        let defid: String = row.get(0).unwrap_or_default();
        let logicalname: String = row.get(1).unwrap_or_default();
        let versiontag: String = row.get(2).unwrap_or_default();
        let repo: String = row.get(3).unwrap_or_default();
        let filepath: String = row.get(4).unwrap_or_default();
        let category: String = row.get(5).unwrap_or_default();
        let description: String = row.get(6).unwrap_or_default();
        let frozen: i64 = row.get(7).unwrap_or(0);

        println!(
            "defid={} logicalname={} version={} repo={} filepath={} category={} frozen={} description={}",
            defid, logicalname, versiontag, repo, filepath, category, frozen, description
        );
    }
}

fn show_restoration_views_phx(conn: &Connection) {
    let sql = r#"
        SELECT nodeid,
               region,
               planeid,
               graphid,
               restorationradius_m,
               restorationradius_hours,
               deltamass_window_kg,
               deltakarma_window,
               gw_risk_max,
               kerband,
               topologygrade,
               nonactuating,
               author_bostrom,
               author_contractid,
               author_comment,
               createdutc
        FROM v_restoration_nodes_phx
        ORDER BY nodeid, createdutc;
    "#;

    let mut stmt = conn
        .prepare(sql)
        .expect("prepare v_restoration_nodes_phx query failed");

    let mut rows = stmt
        .query([])
        .expect("query v_restoration_nodes_phx failed");

    println!("Phoenix-AZ restoration nodes (v_restoration_nodes_phx):");
    print_sep();

    while let Ok(Some(row)) = rows.next() {
        let nodeid: String = row.get(0).unwrap_or_default();
        let region: String = row.get(1).unwrap_or_default();
        let planeid: String = row.get(2).unwrap_or_default();
        let graphid: String = row.get(3).unwrap_or_default();
        let restorationradius_m: f64 = row.get(4).unwrap_or(0.0);
        let restorationradius_hours: f64 = row.get(5).unwrap_or(0.0);
        let deltamass_kg: f64 = row.get(6).unwrap_or(0.0);
        let deltakarma: f64 = row.get(7).unwrap_or(0.0);
        let gw_risk_max: f64 = row.get(8).unwrap_or(0.0);
        let kerband: String = row.get(9).unwrap_or_default();
        let topologygrade: String = row.get(10).unwrap_or_default();
        let nonactuating: i64 = row.get(11).unwrap_or(1);
        let author_bostrom: String = row.get(12).unwrap_or_default();
        let author_contractid: String = row.get(13).unwrap_or_default();
        let author_comment: String = row.get(14).unwrap_or_default();
        let createdutc: String = row.get(15).unwrap_or_default();

        println!(
            "nodeid={} region={} planeid={} graphid={} "
            "restorationradius_m={:.2} restorationradius_hours={:.2} "
            "deltamass_window_kg={:.3} deltakarma_window={:.3} gw_risk_max={:.3} "
            "kerband={} topologygrade={} nonactuating={} "
            "author_bostrom={} author_contractid={} comment={} created={}",
            nodeid,
            region,
            planeid,
            graphid,
            restorationradius_m,
            restorationradius_hours,
            deltamass_kg,
            deltakarma,
            gw_risk_max,
            kerband,
            topologygrade,
            nonactuating,
            author_bostrom,
            author_contractid,
            author_comment,
            createdutc
        );
    }
}

fn show_ecoperjoule_views_phx(conn: &Connection) {
    let sql = r#"
        SELECT nodeid,
               region,
               domain,
               twindowstart,
               twindowend,
               vtresidual,
               kscore,
               escore,
               rscore,
               lane,
               kerdeployable,
               ecoperjoule,
               theta_eco_min,
               carbonnegativeok,
               author_bostrom,
               author_contractid
        FROM v_cyboquatic_ecoperjoule_prod_phx
        ORDER BY nodeid, twindowstart;
    "#;

    let mut stmt = conn
        .prepare(sql)
        .expect("prepare v_cyboquatic_ecoperjoule_prod_phx query failed");

    let mut rows = stmt
        .query([])
        .expect("query v_cyboquatic_ecoperjoule_prod_phx failed");

    println!("Phoenix-AZ eco-per-joule PROD windows (v_cyboquatic_ecoperjoule_prod_phx):");
    print_sep();

    while let Ok(Some(row)) = rows.next() {
        let nodeid: String = row.get(0).unwrap_or_default();
        let region: String = row.get(1).unwrap_or_default();
        let domain: String = row.get(2).unwrap_or_default();
        let tstart: String = row.get(3).unwrap_or_default();
        let tend: String = row.get(4).unwrap_or_default();
        let vtresidual: f64 = row.get(5).unwrap_or(0.0);
        let kscore: f64 = row.get(6).unwrap_or(0.0);
        let escore: f64 = row.get(7).unwrap_or(0.0);
        let rscore: f64 = row.get(8).unwrap_or(0.0);
        let lane: String = row.get(9).unwrap_or_default();
        let kerdeployable: i64 = row.get(10).unwrap_or(0);
        let ecoperjoule: f64 = row.get(11).unwrap_or(0.0);
        let theta_eco_min: f64 = row.get(12).unwrap_or(0.0);
        let carbonnegativeok: i64 = row.get(13).unwrap_or(0);
        let author_bostrom: String = row.get(14).unwrap_or_default();
        let author_contractid: String = row.get(15).unwrap_or_default();

        println!(
            "nodeid={} region={} domain={} window=[{},{}] "
            "Vt={:.3} K={:.3} E={:.3} R={:.3} lane={} kerdeployable={} "
            "ecoperjoule={:.6e} theta_eco_min={:.6e} carbonnegativeok={} "
            "policy_author_bostrom={} policy_contractid={}",
            nodeid,
            region,
            domain,
            tstart,
            tend,
            vtresidual,
            kscore,
            escore,
            rscore,
            lane,
            kerdeployable,
            ecoperjoule,
            theta_eco_min,
            carbonnegativeok,
            author_bostrom,
            author_contractid
        );
    }
}

fn show_mt6883_views_phx(conn: &Connection) {
    let sql = r#"
        SELECT kernelid,
               region,
               lane,
               kscore,
               escore,
               rscore,
               vtmax,
               planesok,
               topologyok,
               mt6883_registry_id,
               mt6883_ok,
               neuroethic_radius_hours,
               neuroethic_ok,
               author_bostrom,
               author_contractid
        FROM v_mt6883_lane_continuity
        WHERE region = 'Phoenix-AZ'
        ORDER BY kernelid;
    "#;

    let mut stmt = conn
        .prepare(sql)
        .expect("prepare v_mt6883_lane_continuity query failed");

    let mut rows = stmt
        .query([])
        .expect("query v_mt6883_lane_continuity failed");

    println!("Phoenix-AZ MT6883 lane continuity and neurorights (v_mt6883_lane_continuity):");
    print_sep();

    while let Ok(Some(row)) = rows.next() {
        let kernelid: String = row.get(0).unwrap_or_default();
        let region: String = row.get(1).unwrap_or_default();
        let lane: String = row.get(2).unwrap_or_default();
        let kscore: f64 = row.get(3).unwrap_or(0.0);
        let escore: f64 = row.get(4).unwrap_or(0.0);
        let rscore: f64 = row.get(5).unwrap_or(0.0);
        let vtmax: f64 = row.get(6).unwrap_or(0.0);
        let planesok: i64 = row.get(7).unwrap_or(0);
        let topologyok: i64 = row.get(8).unwrap_or(0);
        let mt6883_registry_id: i64 = row.get(9).unwrap_or(0);
        let mt6883_ok: i64 = row.get(10).unwrap_or(0);
        let neuroethic_radius_hours: f64 = row.get(11).unwrap_or(0.0);
        let neuroethic_ok: i64 = row.get(12).unwrap_or(0);
        let author_bostrom: String = row.get(13).unwrap_or_default();
        let author_contractid: String = row.get(14).unwrap_or_default();

        println!(
            "kernelid={} region={} lane={} "
            "K={:.3} E={:.3} R={:.3} Vt={:.3} planesok={} topologyok={} "
            "mt6883_registry_id={} mt6883_ok={} "
            "neuroethic_radius_hours={:.2} neuroethic_ok={} "
            "author_bostrom={} author_contractid={}",
            kernelid,
            region,
            lane,
            kscore,
            escore,
            rscore,
            vtmax,
            planesok,
            topologyok,
            mt6883_registry_id,
            mt6883_ok,
            neuroethic_radius_hours,
            neuroethic_ok,
            author_bostrom,
            author_contractid
        );
    }
}

fn main() {
    let opts = parse_args();

    if !opts.show_definitions
        && !opts.show_restoration_views
        && !opts.show_ecoperjoule_views
        && !opts.show_mt6883_views
    {
        eprintln!(
            "Usage: spine_phoenix_governance_probe \\
                [--db PATH] \\
                [--definitions] \\
                [--restoration-phx] \\
                [--ecoperjoule-phx] \\
                [--mt6883-phx]"
        );
        process::exit(1);
    }

    let conn = open_readonly(&opts.db_path);

    if opts.show_definitions {
        show_definitionregistry_phx(&conn);
    }
    if opts.show_restoration_views {
        show_restoration_views_phx(&conn);
    }
    if opts.show_ecoperjoule_views {
        show_ecoperjoule_views_phx(&conn);
    }
    if opts.show_mt6883_views {
        show_mt6883_views_phx(&conn);
    }
}
