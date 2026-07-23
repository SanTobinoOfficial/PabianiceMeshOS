use axum::extract::{Path, State};
use axum::Json;
use serde::{Deserialize, Serialize};
use uuid::Uuid;

use crate::auth::CurrentUser;
use crate::error::ApiError;
use crate::roles::Role;
use crate::state::AppState;

#[derive(Deserialize)]
pub struct CreateCategoryReq {
    pub name: String,
    pub position: Option<i32>,
}

#[derive(Serialize)]
pub struct CategoryResp {
    pub id: Uuid,
    pub name: String,
    pub position: i32,
}

pub async fn create_category(
    State(state): State<AppState>,
    user: CurrentUser,
    Json(req): Json<CreateCategoryReq>,
) -> Result<Json<CategoryResp>, ApiError> {
    user.require(Role::Admin)?;
    let position = req.position.unwrap_or(0);

    let id: Uuid =
        sqlx::query_scalar("insert into categories (name, position) values ($1, $2) returning id")
            .bind(&req.name)
            .bind(position)
            .fetch_one(&state.db)
            .await?;

    Ok(Json(CategoryResp {
        id,
        name: req.name,
        position,
    }))
}

pub async fn list_categories(
    State(state): State<AppState>,
    _user: CurrentUser,
) -> Result<Json<Vec<CategoryResp>>, ApiError> {
    let rows = sqlx::query_as::<_, (Uuid, String, i32)>(
        "select id, name, position from categories order by position asc, name asc",
    )
    .fetch_all(&state.db)
    .await?;

    Ok(Json(
        rows.into_iter()
            .map(|(id, name, position)| CategoryResp { id, name, position })
            .collect(),
    ))
}

#[derive(Deserialize)]
pub struct CreateChannelReq {
    pub name: String,
    pub category_id: Option<Uuid>,
    pub topic: Option<String>,
    pub position: Option<i32>,
    pub min_post_role: Option<Role>,
    pub min_read_role: Option<Role>,
}

#[derive(Serialize)]
pub struct ChannelResp {
    pub id: Uuid,
    pub category_id: Option<Uuid>,
    pub name: String,
    pub topic: Option<String>,
    pub position: i32,
    pub min_post_role: Role,
    pub min_read_role: Role,
}

pub async fn create_channel(
    State(state): State<AppState>,
    user: CurrentUser,
    Json(req): Json<CreateChannelReq>,
) -> Result<Json<ChannelResp>, ApiError> {
    user.require(Role::Admin)?;
    let position = req.position.unwrap_or(0);
    let min_post_role = req.min_post_role.unwrap_or(Role::Member);
    let min_read_role = req.min_read_role.unwrap_or(Role::Member);

    let id: Uuid = sqlx::query_scalar(
        "insert into channels (category_id, name, topic, position, min_post_role, min_read_role)
         values ($1, $2, $3, $4, $5, $6)
         returning id",
    )
    .bind(req.category_id)
    .bind(&req.name)
    .bind(&req.topic)
    .bind(position)
    .bind(min_post_role)
    .bind(min_read_role)
    .fetch_one(&state.db)
    .await?;

    Ok(Json(ChannelResp {
        id,
        category_id: req.category_id,
        name: req.name,
        topic: req.topic,
        position,
        min_post_role,
        min_read_role,
    }))
}

pub async fn list_channels(
    State(state): State<AppState>,
    user: CurrentUser,
) -> Result<Json<Vec<ChannelResp>>, ApiError> {
    let rows = sqlx::query_as::<_, (Uuid, Option<Uuid>, String, Option<String>, i32, Role, Role)>(
        "select id, category_id, name, topic, position, min_post_role, min_read_role from channels
         order by position asc, name asc",
    )
    .fetch_all(&state.db)
    .await?;

    Ok(Json(
        rows.into_iter()
            // kanal ponizej progu roli uzytkownika jest dla niego calkowicie
            // niewidoczny, nie tylko zablokowany do pisania - patrz migracja 0004
            .filter(|(_, _, _, _, _, _, min_read_role)| user.role.has_at_least(*min_read_role))
            .map(
                |(id, category_id, name, topic, position, min_post_role, min_read_role)| {
                    ChannelResp {
                        id,
                        category_id,
                        name,
                        topic,
                        position,
                        min_post_role,
                        min_read_role,
                    }
                },
            )
            .collect(),
    ))
}

pub async fn delete_channel(
    State(state): State<AppState>,
    user: CurrentUser,
    Path(channel_id): Path<Uuid>,
) -> Result<(), ApiError> {
    user.require(Role::Admin)?;

    let result = sqlx::query("delete from channels where id = $1")
        .bind(channel_id)
        .execute(&state.db)
        .await?;

    if result.rows_affected() == 0 {
        return Err(ApiError::NotFound);
    }
    Ok(())
}

#[derive(Deserialize)]
pub struct SetMinPostRoleReq {
    pub min_post_role: Role,
}

pub async fn set_min_post_role(
    State(state): State<AppState>,
    user: CurrentUser,
    Path(channel_id): Path<Uuid>,
    Json(req): Json<SetMinPostRoleReq>,
) -> Result<(), ApiError> {
    user.require(Role::Admin)?;

    let result = sqlx::query("update channels set min_post_role = $1 where id = $2")
        .bind(req.min_post_role)
        .bind(channel_id)
        .execute(&state.db)
        .await?;

    if result.rows_affected() == 0 {
        return Err(ApiError::NotFound);
    }
    Ok(())
}

#[cfg(test)]
mod list_channels_visibility_tests {
    use super::*;
    use sqlx::PgPool;

    async fn make_user(pool: &PgPool, role: Role) -> CurrentUser {
        let id: Uuid = sqlx::query_scalar(
            "insert into users (username, password_hash, role) values ($1, 'x', $2) returning id",
        )
        .bind(format!("user-{}", Uuid::new_v4()))
        .bind(role)
        .fetch_one(pool)
        .await
        .unwrap();
        CurrentUser {
            id,
            username: "test".into(),
            role,
            token_hash: vec![],
        }
    }

    async fn make_channel(pool: &PgPool, name: &str, min_read_role: Role) -> Uuid {
        sqlx::query_scalar(
            "insert into channels (name, min_read_role) values ($1, $2) returning id",
        )
        .bind(name)
        .bind(min_read_role)
        .fetch_one(pool)
        .await
        .unwrap()
    }

    #[sqlx::test]
    async fn member_does_not_see_admin_only_read_channel(pool: PgPool) {
        make_channel(&pool, "jawny", Role::Member).await;
        make_channel(&pool, "tajny", Role::Admin).await;
        let member = make_user(&pool, Role::Member).await;
        let state = AppState { db: pool };

        let result = list_channels(State(state), member).await.unwrap();

        assert_eq!(result.0.len(), 1);
        assert_eq!(result.0[0].name, "jawny");
    }

    #[sqlx::test]
    async fn admin_sees_all_channels(pool: PgPool) {
        make_channel(&pool, "jawny", Role::Member).await;
        make_channel(&pool, "tajny", Role::Admin).await;
        let admin = make_user(&pool, Role::Admin).await;
        let state = AppState { db: pool };

        let result = list_channels(State(state), admin).await.unwrap();

        assert_eq!(result.0.len(), 2);
    }
}

#[derive(Deserialize)]
pub struct SetMinReadRoleReq {
    pub min_read_role: Role,
}

pub async fn set_min_read_role(
    State(state): State<AppState>,
    user: CurrentUser,
    Path(channel_id): Path<Uuid>,
    Json(req): Json<SetMinReadRoleReq>,
) -> Result<(), ApiError> {
    user.require(Role::Admin)?;

    let result = sqlx::query("update channels set min_read_role = $1 where id = $2")
        .bind(req.min_read_role)
        .bind(channel_id)
        .execute(&state.db)
        .await?;

    if result.rows_affected() == 0 {
        return Err(ApiError::NotFound);
    }
    Ok(())
}
