//! This module contains Hjson-specific encodings of certificate template
//! components.
//!
//! They are kept separated here so that details of the representation of
//! templates on-disk (in Hjson) can change without the API of the templat
//! structs changing.

use hex::FromHex;
use num_bigint_dig::BigUint as InnerBigUint;
use num_traits::{Num, ToPrimitive};
use serde::de;
use serde::{Deserialize, Deserializer};
use serde_with::DeserializeAs;

use crate::template;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct BigUint {
    biguint: InnerBigUint,
}

impl From<BigUint> for InnerBigUint {
    fn from(value: BigUint) -> InnerBigUint {
        value.biguint
    }
}

// Helper trait to improve error messages for Value<T>.
pub trait DeserializeAsHelpMsg<T> {
    fn help_msg() -> String;
    fn example() -> String;
}

impl DeserializeAsHelpMsg<String> for String {
    fn help_msg() -> String {
        "a string".to_string()
    }

    fn example() -> String {
        "\"hello wrodl\"".to_string()
    }
}

// Deserialize BigUint as InnerBigUint. This is necessary because the default hjson
// parser really wants to parse numbers by itself and it won't handle big integers
// or any customization.
impl<'de> DeserializeAs<'de, InnerBigUint> for BigUint {
    fn deserialize_as<D>(deserializer: D) -> Result<InnerBigUint, D::Error>
    where
        D: Deserializer<'de>,
    {
        deserializer
            .deserialize_str(BigUintVisitor)
            .map(InnerBigUint::from)
    }
}

impl DeserializeAsHelpMsg<InnerBigUint> for BigUint {
    fn help_msg() -> String {
        "a string representing a non-negative integer (you can use the prefix '0x' for hexadecimal)"
            .to_string()
    }

    fn example() -> String {
        "410983 or 0x64567".to_string()
    }
}

// Same for u8, derserialize as BigUint and see if it fits.
impl<'de> DeserializeAs<'de, u8> for BigUint {
    fn deserialize_as<D>(deserializer: D) -> Result<u8, D::Error>
    where
        D: Deserializer<'de>,
    {
        let inner: InnerBigUint = BigUint::deserialize_as(deserializer)?;
        match inner.to_u8() {
            Some(x) => Ok(x),
            None => Err(de::Error::custom(format!(
                "expected 8-bit integer but {} is too large",
                inner
            ))),
        }
    }
}

impl DeserializeAsHelpMsg<u8> for BigUint {
    fn help_msg() -> String {
        "a string representing a non-negative 8-bit integer (you can use the prefix '0x' for hexadecimal)".to_string()
    }

    fn example() -> String {
        "123 or 0x7b".to_string()
    }
}

struct BigUintVisitor;

impl<'de> serde::de::Visitor<'de> for BigUintVisitor {
    type Value = BigUint;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("a non-negative integer, you can use the prefix '0x' for hexadecimal")
    }

    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E>
    where
        E: de::Error,
    {
        // Unless the string starts with '0x', expect a decimal string.
        let (radix, s) = s.strip_prefix("0x").map_or_else(|| (10, s), |s| (16, s));
        InnerBigUint::from_str_radix(s, radix)
            .map_err(de::Error::custom)
            .map(|biguint| BigUint { biguint })
    }
}

pub struct HexString;

/// Deserialization of a `Value<Vec<u8>>` from a string of hex digits.
impl<'de> DeserializeAs<'de, Vec<u8>> for HexString {
    fn deserialize_as<D>(deserializer: D) -> Result<Vec<u8>, D::Error>
    where
        D: Deserializer<'de>,
    {
        let s = String::deserialize(deserializer)?;
        Vec::<u8>::from_hex(s)
            .map_err(|err| de::Error::custom(format!("could not parse hexstring: {}", err)))
    }
}

impl DeserializeAsHelpMsg<Vec<u8>> for HexString {
    fn help_msg() -> String {
        "a hexstring (a string of hexadecimal characters representing a byte array)".to_string()
    }

    fn example() -> String {
        "ff8702".to_string()
    }
}

impl<T> DeserializeAsHelpMsg<T> for serde_with::Same
where
    T: DeserializeAsHelpMsg<T>,
{
    fn help_msg() -> String {
        T::help_msg()
    }

    fn example() -> String {
        T::example()
    }
}

/// Declaration of a variable that can be filled into the template.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Deserialize)]
#[serde(tag = "type", rename_all = "kebab-case")]
pub enum VariableType {
    /// Raw array of bytes.
    ByteArray {
        /// Length in bytes for this variable.
        size: usize,
    },
    /// Signed integer: such an integer is represented by an array of
    /// of small-size CPU word called limbs. In little-endian mode,
    /// the words are ordered from the least significant to the most
    /// significant, and the opposite for big-endian.
    /// As a special case, small integers (8, 16, 32 and 64 bits) can
    /// be specified by omitting both the endianness and limb_size.
    Integer {
        /// Maximum size in bytes for this variable.
        size: usize,
        // Endianness: optional for small integers (8, 16, 32 and 64).
        endianness: Option<template::Endianness>,
        // Limb size: optional for small integers. If specified, must divide
        // the size.
        limb_size: Option<usize>,
    },
    /// UTF-8 encoded String.
    String {
        /// Maximum size in bytes for this variable.
        size: usize,
    },
}

// Deserialize VariableType and handle some of the optional part to make the hjson
// nice but avoid Option in the internal representation.
impl<'de> DeserializeAs<'de, template::VariableType> for VariableType {
    fn deserialize_as<D>(deserializer: D) -> Result<template::VariableType, D::Error>
    where
        D: Deserializer<'de>,
    {
        match VariableType::deserialize(deserializer)? {
            VariableType::ByteArray { size } => Ok(template::VariableType::ByteArray { size }),
            VariableType::String { size } => Ok(template::VariableType::String { size }),
            VariableType::Integer {
                size,
                endianness,
                limb_size,
            } => {
                match endianness {
                    None => {
                        // The endianness can only be omitted for small integers, in which case the limb_size must also be omitted.
                        if limb_size.is_some() {
                            return Err(de::Error::custom(
                                "the endianness must be specified if the limb size is specified",
                            ));
                        }
                        // If both endianness and limb are unspecified, it must be small integers in which case we assume that
                        // the limb size is the whole size.
                        let small_integers = [1usize, 2, 4, 8];
                        if !small_integers.contains(&size) {
                            return Err(de::Error::custom("only small integers (1, 2, 4 and 8 byts) can omit the endianness and limb size"));
                        }
                        Ok(template::VariableType::Integer {
                            size,
                            endianness: template::Endianness::Little,
                            limb_size: size,
                        })
                    }
                    Some(endianness) => {
                        // If the limb size is omitted, assume that it is 1. Otherwise, check that it divides the size.
                        let limb_size = limb_size.unwrap_or(1);
                        if (size % limb_size) != 0 {
                            return Err(de::Error::custom(
                                "the size must be a multiple of the limb size",
                            ));
                        }
                        Ok(template::VariableType::Integer {
                            size,
                            endianness,
                            limb_size,
                        })
                    }
                }
            }
        }
    }
}
