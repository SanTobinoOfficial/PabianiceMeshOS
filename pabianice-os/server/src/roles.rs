use serde::{Deserialize, Serialize};

// Kolejnosc wariantow ma znaczenie - Ord/PartialOrd porownuje po niej (Member < Moderator
// < Admin), na tym opiera sie has_at_least. System ról i uprawnień (rozdz. 10.4.1 planu) -
// w tym szkielecie calo-serwerowa, bez override per-kanal (patrz pabianice-os/README.md).
#[derive(
    Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize, sqlx::Type,
)]
#[sqlx(type_name = "user_role", rename_all = "lowercase")]
#[serde(rename_all = "lowercase")]
pub enum Role {
    Member,
    Moderator,
    Admin,
}

impl Role {
    pub fn has_at_least(self, min: Role) -> bool {
        self >= min
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn admin_has_all_levels() {
        assert!(Role::Admin.has_at_least(Role::Admin));
        assert!(Role::Admin.has_at_least(Role::Moderator));
        assert!(Role::Admin.has_at_least(Role::Member));
    }

    #[test]
    fn member_only_has_member() {
        assert!(Role::Member.has_at_least(Role::Member));
        assert!(!Role::Member.has_at_least(Role::Moderator));
        assert!(!Role::Member.has_at_least(Role::Admin));
    }

    #[test]
    fn moderator_is_between() {
        assert!(Role::Moderator.has_at_least(Role::Member));
        assert!(Role::Moderator.has_at_least(Role::Moderator));
        assert!(!Role::Moderator.has_at_least(Role::Admin));
    }
}
