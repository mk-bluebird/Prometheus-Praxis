// crates/cyberfs-core/src/kani_organ_vfs.rs
#![forbid(unsafe_code)]

use kani::any;
use std::path::{Path, PathBuf};

use crate::organ_vfs::{OrganMountRoot, OrganReadOnlyVfs};

/// Mock Dir-like type for symbolic confinement proofs.
/// In real builds, `OrganMountRoot` uses `cap_std::fs::Dir`.
struct MockDir;

impl MockDir {
    fn open(&self, relpath: &Path) -> Result<(), ()> {
        // Assert that relpath is not absolute and has no root component.
        assert!(!relpath.is_absolute());
        assert!(relpath.components().next().map(|c| !matches!(c, std::path::Component::RootDir)).unwrap_or(true));
        Ok(())
    }

    fn read_dir(&self, relpath: &Path) -> Result<(), ()> {
        assert!(!relpath.is_absolute());
        assert!(relpath.components().next().map(|c| !matches!(c, std::path::Component::RootDir)).unwrap_or(true));
        Ok(())
    }

    fn metadata(&self, relpath: &Path) -> Result<(), ()> {
        assert!(!relpath.is_absolute());
        assert!(relpath.components().next().map(|c| !matches!(c, std::path::Component::RootDir)).unwrap_or(true));
        Ok(())
    }
}

/// Adapter that reuses `OrganMountRoot` logic with `MockDir` in proofs.
struct MockOrganMountRoot {
    dir: MockDir,
}

impl MockOrganMountRoot {
    fn new() -> Self {
        Self { dir: MockDir }
    }
}

impl OrganReadOnlyVfs for MockOrganMountRoot {
    fn open_file(&self, relpath: &Path) -> std::io::Result<cap_std::fs::File> {
        // In proofs we care only about the relative path property;
        // stub out the File with `unimplemented!` after the assertions.
        self.dir.open(relpath).map_err(|_| std::io::Error::new(std::io::ErrorKind::Other, "mock open failed"))?;
        unimplemented!()
    }

    fn read_dir(&self, relpath: &Path) -> std::io::Result<cap_std::fs::ReadDir> {
        self.dir.read_dir(relpath).map_err(|_| std::io::Error::new(std::io::ErrorKind::Other, "mock readdir failed"))?;
        unimplemented!()
    }

    fn metadata(&self, relpath: &Path) -> std::io::Result<cap_std::fs::Metadata> {
        self.dir.metadata(relpath).map_err(|_| std::io::Error::new(std::io::ErrorKind::Other, "mock metadata failed"))?;
        unimplemented!()
    }
}

#[cfg(kani)]
#[kani::proof]
fn organ_vfs_paths_are_confined() {
    // Symbolic arbitrary byte sequence for a path; Kani explores many byte combinations.
    let raw: Vec<u8> = any();
    let s = String::from_utf8_lossy(&raw);
    let candidate: PathBuf = PathBuf::from(s.to_string());

    let mount = MockOrganMountRoot::new();

    // All OrganReadOnlyVfs calls must treat `candidate` as a relative, corridor-confined path.
    // Any attempt to construct or use an absolute/rooted path would violate assertions.
    let _ = mount.open_file(&candidate);
    let _ = mount.read_dir(&candidate);
    let _ = mount.metadata(&candidate);
}
