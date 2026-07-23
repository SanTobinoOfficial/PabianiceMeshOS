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
}

#[derive(Serialize)]
pub struct ChannelResp {
    pub id: Uuid,
    pub category_id: Option<Uuid>,
    pub name: String,
    pub topic: Option<String>,
    pub position: i32,
}

pub async fn create_channel(
    State(state): State<AppState>,
    user: CurrentUser,
    Json(req): Json<CreateChannelReq>,
) -> Result<Json<ChannelResp>, ApiError> {
    user.require(Role::Admin)?;
    let position = req.position.unwrap_or(0);

    let id: Uuid = sqlx::query_scalar(
        "insert into channels (category_id, name, topic, position) values ($1, $2, $3, $4)
         returning id",
    )
    .bind(req.category_id)
    .bind(&req.name)
    .bind(&req.topic)
    .bind(position)
    .fetch_one(&state.db)
    .await?;

    Ok(Json(ChannelResp {
        id,
        category_id: req.category_id,
        name: req.name,
        topic: req.topic,
        position,
    }))
}

pub async fn list_channels(
    State(state): State<AppState>,
    _user: CurrentUser,
) -> Result<Json<Vec<ChannelResp>>, ApiError> {
    let rows = sqlx::query_as::<_, (Uuid, Option<Uuid>, String, Option<String>, i32)>(
        "select id, category_id, name, topic, position from channels
         order by position asc, name asc",
    )
    .fetch_all(&state.db)
    .await?;

    Ok(Json(
        rows.into_iter()
            .map(|(id, category_id, name, topic, position)| ChannelResp {
                id,
                category_id,
                name,
                topic,
                position,
            })
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
