//! GorpOS kernel core — Rust mirror of `src/kernel/`.
//!
//! Faithful port of the C logic: packed variable-width cells, the
//! dynamic bit-width allocator, and kernel-native ternary logic.
//! Uses only `core`, so it can be compiled `no_std` for a real target.
//! (DOS loader and boot sector remain C/asm-only for now.)

#![no_std]

pub mod cell;
pub mod alloc;
pub mod trit;
