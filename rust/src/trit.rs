//! Kernel-native ternary boolean (Kleene logic).
//! Mirror of `src/kernel/trit.h`.

/// 2-bit encoding: 0 = false, 1 = unknown, 2 = true. 3 is reserved.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum Trit {
    False = 0,
    Unknown = 1,
    True = 2,
}

impl Trit {
    pub fn not(self) -> Trit {
        match self {
            Trit::False => Trit::True,
            Trit::True => Trit::False,
            Trit::Unknown => Trit::Unknown,
        }
    }

    pub fn and(self, other: Trit) -> Trit {
        if self == Trit::False || other == Trit::False {
            return Trit::False;
        }
        if self == Trit::True && other == Trit::True {
            return Trit::True;
        }
        Trit::Unknown
    }

    pub fn or(self, other: Trit) -> Trit {
        if self == Trit::True || other == Trit::True {
            return Trit::True;
        }
        if self == Trit::False && other == Trit::False {
            return Trit::False;
        }
        Trit::Unknown
    }

    /// Binary compat: unknown -> false (or trap if STRICT_TERNARY).
    pub fn to_bool(self) -> bool {
        self == Trit::True
    }

    pub fn from_bool(b: bool) -> Trit {
        if b {
            Trit::True
        } else {
            Trit::False
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn kleene_tables() {
        use Trit::*;
        assert_eq!(True.and(True), True);
        assert_eq!(True.and(Unknown), Unknown);
        assert_eq!(True.and(False), False);
        assert_eq!(Unknown.and(False), False);
        assert_eq!(False.or(False), False);
        assert_eq!(False.or(Unknown), Unknown);
        assert_eq!(True.or(Unknown), True);
        assert_eq!(Unknown.not(), Unknown);
        assert_eq!(False.not(), True);
    }

    #[test]
    fn bool_roundtrip() {
        assert!(Trit::True.to_bool());
        assert!(!Trit::False.to_bool());
        assert!(!Trit::Unknown.to_bool()); // unknown -> false
        assert_eq!(Trit::from_bool(true), Trit::True);
        assert_eq!(Trit::from_bool(false), Trit::False);
    }
}
