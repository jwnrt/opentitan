//! This module is capable of generating C code for manipulating a binary X.509
//! certificate according to a [`Template`](crate::template::Template).

use crate::offsets::{CertificateOffset, CertificateVariable};
use crate::template::{Conversion, VariableType};

/// C code generator for setting values of a binary X509 certificate.
pub struct CGenerator;

impl CGenerator {
    /// Generate a certificate manipulation function for a given variable.
    ///
    /// This C function will accept a value of the source type for the variable
    /// and perform the conversions and writes needed for each of its use-sites.
    /// This function returns two strings: the first one is the C code of the implementation,
    /// the second is the prototype for the header definition.
    pub fn generate_fn(cert_name: &str, cert_variable: &CertificateVariable) -> (String, String) {
        let CertificateVariable {
            name,
            source_type,
            offsets,
        } = cert_variable;

        let c_offset_writes = offsets
            .iter()
            .map(|offset| Self::generate_offset_write(name, source_type, offset))
            .collect::<Vec<_>>()
            .join("\n\t");

        let value_decl = Self::c_variable_decl(name, source_type);
        let c_source = indoc::formatdoc! {r#"
            status_t {cert_name}_set_{name}({value_decl}) {{
                {c_offset_writes}
            }}
        "#};

        let h_def = format!("status_t {cert_name}_set_{name}({value_decl});");

        (c_source, h_def)
    }

    /// Generate the C statements required to set a use-site of a variable at
    /// some offset within the certificate.
    ///
    /// This C code includes conversions from the source to the target type.
    pub fn generate_offset_write(
        source_var_name: &String,
        source_type: &VariableType,
        cert_offset: &CertificateOffset,
    ) -> String {
        let CertificateOffset {
            conversion,
            offset,
            target_type,
            ..
        } = cert_offset;

        let source_size = source_type.size();
        let target_size = target_type.size();

        let target_var_name = format!("{source_var_name}_der");
        let target_var_decl = Self::c_variable_decl(&target_var_name, target_type);

        let c_conversion = match conversion {
            Some(conv) => {
                let conv_fn = match conv {
                    Conversion::LowercaseHex => {
                        "ot_cert_convert_byte_array_to_string_lowercase_hex"
                    }
                    Conversion::LittleEndian => {
                        "ot_cert_convert_byte_array_to_integer_little_endian"
                    }
                    Conversion::BigEndian => "ot_cert_convert_byte_array_to_integer_big_endian",
                };
                format!("{conv_fn}({source_var_name}, {source_size}, &{target_var_name}, {target_size}, {offset})")
            }
            None => format!("target_var_name = {source_var_name}"),
        };

        indoc::formatdoc! {r#"
            {target_var_decl};
            {c_conversion};
            memcpy((uint8_t *)ot_cert + offset, &{target_var_name}, {target_size});
        "#}
    }

    fn c_integer_for_size(size: usize) -> &'static str {
        match size {
            1 => "int8_t",
            2 => "int16_t",
            4 => "int32_t",
            8 => "int64_t",
            16 => "int128_t",
            _ => unimplemented!("integers of size {size} not supported"),
        }
    }

    fn c_variable_decl(name: &str, var_type: &VariableType) -> String {
        match var_type {
            VariableType::ByteArray { size } => format!("uint8_t {name}[{size}]"),
            VariableType::Integer { size, .. } => {
                let c_type = Self::c_integer_for_size(*size);
                format!("{c_type} {name}")
            }
            VariableType::String { size } => format!("char {name}[{size}]"),
        }
    }
}
