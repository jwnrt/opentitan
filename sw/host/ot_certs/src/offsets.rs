// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

use anyhow::{bail, Context, Result};
use std::collections::HashMap;

use crate::template::{Conversion, Value, VariableType};
use crate::types::{convert_type, Type};

#[derive(Debug, Clone)]
pub struct CertificateOffset {
    /// Offset into the DER.
    pub offset: usize,
    /// Type of the variable in the DER.
    pub target_type: VariableType,
    /// Conversion that needs to be applied to the original variable.
    pub conversion: Option<Conversion>,
}

#[derive(Debug, Clone)]
pub struct CertificateVariable {
    pub name: String,
    pub source_type: VariableType,
    pub offsets: Vec<CertificateOffset>,
}

/// X509 certificate in DER form where the offset to variables has been recorded.
#[derive(Debug, Clone)]
pub struct CertificateWithVariables {
    /// Name of the certificate, will be used to generate names in the template.
    pub name: String,
    /// Certificate in binary form.
    pub cert: Vec<u8>,
    /// List of offsets where the variables are used. A variable can appear multiple
    /// times with different types.
    pub variables: Vec<CertificateVariable>,
}

struct GeneratorVariable {
    hidden: bool,
    source_type: VariableType,
    values: Vec<Vec<u8>>,
    offsets: Vec<CertificateOffset>,
}

impl GeneratorVariable {
    fn new(source_type: VariableType) -> GeneratorVariable {
        GeneratorVariable {
            hidden: false,
            source_type,
            values: Vec::new(),
            offsets: Vec::new(),
        }
    }
}

// The generators holds the information about the offset to the variables
// in the certificate and the randomly generated values that we used during generation.
pub struct OffsetGenerator {
    variables: HashMap<String, GeneratorVariable>,
}

// Super inefficient function to search for needle in a haystack.
fn find_subsequence(haystack: &[u8], needle: &[u8]) -> Option<usize> {
    haystack
        .windows(needle.len())
        .position(|window| window == needle)
}

impl OffsetGenerator {
    pub fn new(variables: &HashMap<String, VariableType>) -> OffsetGenerator {
        let variables = variables
            .iter()
            .map(|(name, source_type)| (name.clone(), GeneratorVariable::new(*source_type)))
            .collect();
        OffsetGenerator { variables }
    }

    pub fn add_variable<T>(&mut self, bytes: Vec<u8>, target_type: VariableType, val: &Value<T>) {
        match val {
            Value::Literal(_) => {
                panic!("add_variable can only be called on variables, not literals")
            }
            Value::Variable { var: name, convert } => {
                // Register value.
                if let Some(entry) = self.variables.get_mut(name) {
                    entry.values.push(bytes);
                    // Register offset.
                    entry.offsets.push(CertificateOffset {
                        offset: 0,
                        target_type,
                        conversion: *convert,
                    });
                } else {
                    // This should never occur because the variable name should have been checked before
                    // in get_value.
                    panic!("variable '{}' does not exist", name);
                }
            }
        }
    }

    pub fn add_hidden_variable(&mut self, name: String, bytes: Vec<u8>) {
        let target_type = VariableType::ByteArray { size: bytes.len() };
        let off = CertificateOffset {
            offset: 0,
            target_type,
            conversion: None,
        };
        let var = GeneratorVariable {
            hidden: true,
            source_type: target_type,
            values: vec![bytes],
            offsets: vec![off],
        };
        self.variables.insert(name, var);
    }

    fn compute_offsets(
        &self,
        cert: &mut [u8],
        clear_variables: bool,
    ) -> Result<Vec<CertificateVariable>> {
        let mut res = Vec::new();
        for (name, var) in self.variables.iter() {
            let mut offsets = Vec::new();
            for (i, offset) in var.offsets.iter().enumerate() {
                let needle = &var.values[i];
                match find_subsequence(cert, needle) {
                    Some(pos) => {
                        if clear_variables {
                            cert[pos..pos + offset.target_type.size()].fill(0);
                        }
                        let mut new_offset = offset.clone();
                        new_offset.offset = pos;
                        offsets.push(new_offset);
                    }
                    None => bail!("internal error: could not find the offset of {:?} in the binary certificate, the value should be {:?}", offset, needle),
                }
            }
            // Ignore hidden variables.
            if !var.hidden {
                res.push(CertificateVariable {
                    name: name.clone(),
                    source_type: var.source_type,
                    offsets,
                })
            }
        }
        Ok(res)
    }

    pub fn generate(
        &self,
        name: String,
        mut cert: Vec<u8>,
        clear_variables: bool,
    ) -> Result<CertificateWithVariables> {
        // Compute the offsets and clear variables.
        let variables = self.compute_offsets(&mut cert, clear_variables)?;
        Ok(CertificateWithVariables {
            name,
            cert,
            variables,
        })
    }

    // Helper function: take a Value<T> and return a T or an error. If the value
    // was a variable, a random one will be generated and registerd with the
    // generator. Any conversion operator will be checked and handled there.
    pub fn get_value<T: Clone>(&mut self, val: &Value<T>) -> Result<T>
    where
        T: Type<T>,
    {
        match &val {
            Value::Literal(x) => Ok(x.clone()),
            Value::Variable { var, convert } => {
                // Lookup variable
                let source_type = self
                    .variables
                    .get(var)
                    .with_context(|| format!("variable '{}' does not exist", var))?
                    .source_type;
                // Convert type if necessary.
                let target_type = convert_type(&source_type, &T::variable_type(), convert)
                    .with_context(|| {
                        format!(
                            "variable '{}' cannot be converted to the type of this field",
                            var
                        )
                    })?;
                let (bytes, value) = T::random_value(target_type.size());
                self.add_variable(bytes, target_type, val);
                Ok(value)
            }
        }
    }
}
