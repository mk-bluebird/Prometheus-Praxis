// crates/cyberfs-core/src/organ_vfs.rs
#![forbid(unsafe_code)]

use std::io;
use std::path::Path;

use cap_std::fs::{Dir, File, Metadata, ReadDir};

/// Capability representing a single, read-only organ corridor mount root.
/// All paths are resolved relative to this root via `cap_std::fs::Dir`.
#[derive(Debug)]
pub struct OrganMountRoot {
    dir: Dir,
    /// Logical corridor name, e.g. "renal.corridor.v1.aln".
    corridor_id: String,
}

impl OrganMountRoot {
    /// Construct from an existing `Dir` capability created by the host.
    /// Host code is responsible for ensuring `dir` points at the renal corridor root.
    pub fn new(dir: Dir, corridor_id: impl Into<String>) -> Self {
        Self {
            dir,
            corridor_id: corridor_id.into(),
        }
    }

    /// Corridor identifier accessor.
    pub fn corridor_id(&self) -> &str {
        &self.corridor_id
    }
}

/// Read-only VFS operations confined to the organ mount root.
/// This trait is the only filesystem surface CyberFS-core is allowed to use.
pub trait OrganReadOnlyVfs {
    /// Open a file for read-only access, by a relative path under the organ corridor.
    fn open_file(&self, relpath: &Path) -> io::Result<File>;

    /// Read directory entries under the given relative path.
    fn read_dir(&self, relpath: &Path) -> io::Result<ReadDir>;

    /// Fetch metadata (read-only) for a path under the organ corridor root.
    fn metadata(&self, relpath: &Path) -> io::Result<Metadata>;
}

impl OrganReadOnlyVfs for OrganMountRoot {
    fn open_file(&self, relpath: &Path) -> io::Result<File> {
        // cap-std's Dir::open resolves relpath relative to this directory
        // and prevents escaping the corridor root.
        self.dir.open(relpath)
    }

    fn read_dir(&self, relpath: &Path) -> io::Result<ReadDir> {
        self.dir.read_dir(relpath)
    }

    fn metadata(&self, relpath: &Path) -> io::Result<Metadata> {
        self.dir.metadata(relpath)
    }
}
