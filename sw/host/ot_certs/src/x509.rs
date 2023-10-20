// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

use anyhow::{bail, Context, Result};
use num_bigint_dig::BigUint;
use openssl::asn1::{Asn1Integer, Asn1Object, Asn1OctetString, Asn1Time};
use openssl::bn::{BigNum, BigNumContext};
use openssl::ec::{EcGroup, EcKey};
use openssl::ecdsa::EcdsaSig;
use openssl::hash::MessageDigest;
use openssl::nid::Nid;
use openssl::pkey::PKey;
use openssl::pkey::{Private, Public};
use openssl::x509::extension;
use openssl::x509::{X509Extension, X509NameBuilder, X509};

use crate::offsets::{CertificateWithVariables, OffsetGenerator};
use crate::template;

struct Algorithm {
    key_pair: PKey<Private>,
    hash: MessageDigest,
}

fn ecgroup_from_curve(curve: &template::EcCurve) -> Result<EcGroup> {
    match curve {
        template::EcCurve::Prime256v1 => Ok(EcGroup::from_curve_name(Nid::X9_62_PRIME256V1)?),
    }
}

impl Algorithm {
    // Generate new random ecdsa private/pub key pair
    fn new_ecdsa_with_hash(curve: &template::EcCurve, hash: MessageDigest) -> Result<Algorithm> {
        let group = ecgroup_from_curve(curve)?;
        let key = EcKey::<Private>::generate(&group)?;
        let key_pair = PKey::try_from(key)?;
        Ok(Algorithm { key_pair, hash })
    }
}

fn bignum_from_bigint(bigint: &BigUint) -> BigNum {
    BigNum::from_slice(&bigint.to_bytes_be()).unwrap()
}

fn get_signature_algorithm(alg: &template::Signature) -> Result<Algorithm> {
    match alg {
        template::Signature::EcdsaWithSha256 { .. } => Ok(Algorithm::new_ecdsa_with_hash(
            &template::EcCurve::Prime256v1,
            MessageDigest::sha256(),
        )?),
    }
}

fn get_ec_pubkey(
    generator: &mut OffsetGenerator,
    pubkey_info: &template::EcPublicKeyInfo,
) -> Result<PKey<Public>> {
    // The template can either specify both x and y as literal, or as variable but we don't support a mix for both.
    let group = ecgroup_from_curve(&pubkey_info.curve)?;
    match (&pubkey_info.public_key.x, &pubkey_info.public_key.y) {
        (template::Value::Literal(x), template::Value::Literal(y)) => {
            let key = EcKey::<Public>::from_public_key_affine_coordinates(
                &group,
                &bignum_from_bigint(x),
                &bignum_from_bigint(y),
            )
            .context("could not create a public key from provided curve and x,y coordinates")?;
            Ok(PKey::try_from(key)?)
        }
        (template::Value::Variable { .. }, template::Value::Variable { .. }) => {
            // We cannot use random x and y since they need to be on the curve. Generate a random one and
            // register it with the generator.
            let privkey = EcKey::<Private>::generate(&group)?;
            let pubkey = EcKey::<Public>::from_public_key(&group, privkey.public_key())?;
            let mut ctx = BigNumContext::new()?;
            let mut x = BigNum::new()?;
            let mut y = BigNum::new()?;
            privkey
                .public_key()
                .affine_coordinates(&group, &mut x, &mut y, &mut ctx)?;
            // Convert x and y to the DER representation, potentially adding some padding if necessary.
            // FIXME avoid constant for 32
            let target_type = template::VariableType::Integer {
                size: 32,
                endianness: template::Endianness::Big,
                limb_size: 1,
            };
            generator.add_variable(x.to_vec_padded(32)?, target_type, &pubkey_info.public_key.x);
            generator.add_variable(y.to_vec_padded(32)?, target_type, &pubkey_info.public_key.y);
            Ok(PKey::try_from(pubkey)?)
        }
        _ => bail!("you cannot mix literals and variables in the public key for x and y"),
    }
}

fn get_pubkey(
    generator: &mut OffsetGenerator,
    pubkey_info: &template::SubjectPublicKeyInfo,
) -> Result<PKey<Public>> {
    match pubkey_info {
        template::SubjectPublicKeyInfo::EcPublicKey(ec_pubkey) => {
            get_ec_pubkey(generator, ec_pubkey)
        }
    }
}

fn subject_key_id_extension(keyid: &[u8]) -> Result<X509Extension> {
    // From https://datatracker.ietf.org/doc/html/rfc5280#section-4.2.1
    // id-ce   OBJECT IDENTIFIER ::=  { joint-iso-ccitt(2) ds(5) 29 }
    //
    // From https://datatracker.ietf.org/doc/html/rfc5280#section-4.2.1.2
    // id-ce-subjectKeyIdentifier OBJECT IDENTIFIER ::=  { id-ce 14 }
    // SubjectKeyIdentifier ::= KeyIdentifier
    //
    // From https://datatracker.ietf.org/doc/html/rfc5280#section-4.2.1.1
    // KeyIdentifier ::= OCTET STRING
    let der = asn1::write(|w| w.write_element(&keyid))?;
    let octet_string = Asn1OctetString::new_from_bytes(&der)?;
    // Unfortunately, the rust binding does not seem to allow creating a Asn1Object
    // from a Nid so we have to manually create it from the OID string.
    let oid = Asn1Object::from_str("2.5.29.14")?;
    Ok(X509Extension::new_from_der(&oid, false, &octet_string)?)
}

// FIXME does not support any other fields of authorityKeyIdentifier for now
fn auth_key_id_extension(keyid: &[u8]) -> Result<X509Extension> {
    // From https://datatracker.ietf.org/doc/html/rfc5280#section-4.2.1
    // id-ce   OBJECT IDENTIFIER ::=  { joint-iso-ccitt(2) ds(5) 29 }
    //
    // From https://datatracker.ietf.org/doc/html/rfc5280#section-4.2.1.1
    // id-ce-authorityKeyIdentifier OBJECT IDENTIFIER ::=  { id-ce 35 }
    //
    // AuthorityKeyIdentifier ::= SEQUENCE {
    // keyIdentifier             [0] KeyIdentifier           OPTIONAL,
    // authorityCertIssuer       [1] GeneralNames            OPTIONAL,
    // authorityCertSerialNumber [2] CertificateSerialNumber OPTIONAL  }
    //
    // KeyIdentifier ::= OCTET STRING
    //
    // Note: this is part of the implicit tagged modules:
    // https://datatracker.ietf.org/doc/html/rfc5280#appendix-A.2
    #[derive(asn1::Asn1Write)]
    struct AuthorityKeyIdentifier<'a> {
        #[implicit(0)]
        keyid: Option<&'a [u8]>,
        // FIXME add more fields here if necessary
    }

    let der = asn1::write_single(&AuthorityKeyIdentifier { keyid: Some(keyid) })?;
    let octet_string = Asn1OctetString::new_from_bytes(&der)?;
    // Unfortunately, the rust binding does not seem to allow creating a Asn1Object
    // from a Nid so we have to manually create it from the OID string.
    let oid = Asn1Object::from_str("2.5.29.35")?;
    Ok(X509Extension::new_from_der(&oid, false, &octet_string)?)
}

// From DICE specification:
// https://trustedcomputinggroup.org/wp-content/uploads/DICE-Attestation-Architecture-r23-final.pdf
//
// tcg OBJECT IDENTIFIER ::= {2 23 133}
// tcg-dice OBJECT IDENTIFIER ::= { tcg platformClass(5) 4 }
// tcg-dice-TcbInfo OBJECT IDENTIFIER ::= {tcg-dice 1}
// DiceTcbInfo ::== SEQUENCE {
//     vendor [0] IMPLICIT UTF8String OPTIONAL,
//     model [1] IMPLICIT UTF8String OPTIONAL,
//     version [2] IMPLICIT UTF8String OPTIONAL,
//     svn [3] IMPLICIT INTEGER OPTIONAL,
//     layer [4] IMPLICIT INTEGER OPTIONAL,
//     index [5] IMPLICIT INTEGER OPTIONAL,
//     fwids [6] IMPLICIT FWIDLIST OPTIONAL,
//     flags [7] IMPLICIT OperationalFlags OPTIONAL,
//     vendorInfo [8] IMPLICIT OCTET STRING OPTIONAL,
//     type [9] IMPLICIT OCTET STRING OPTIONAL
// }
// FWIDLIST ::== SEQUENCE SIZE (1..MAX) OF FWID
//     FWID ::== SEQUENCE {
//     hashAlg OBJECT IDENTIFIER,
//     digest OCTET STRING
// }
// OperationalFlags ::= BIT STRING {
//     notConfigured (0),
//     notSecure (1),
//     recovery (2),
//     debug (3)
// }
#[derive(asn1::Asn1Write)]
struct Fwid<'a> {
    hash_alg: asn1::ObjectIdentifier,
    digest: &'a [u8],
}

#[derive(asn1::Asn1Write)]
struct DiceTcbInfo<'a> {
    #[implicit(0)]
    vendor: Option<asn1::Utf8String<'a>>,
    #[implicit(1)]
    model: Option<asn1::Utf8String<'a>>,
    #[implicit(2)]
    version: Option<asn1::Utf8String<'a>>,
    #[implicit(3)]
    svn: Option<asn1::BigInt<'a>>,
    #[implicit(4)]
    layer: Option<asn1::BigInt<'a>>,
    #[implicit(5)]
    index: Option<asn1::BigInt<'a>>,
    #[implicit(6)]
    //fwids: Option<&'a [Fwid<'a>]>,#
    fwids: Option<asn1::SequenceOfWriter<'a, Fwid<'a>>>,
    #[implicit(7)]
    flags: Option<asn1::BitString<'a>>,
    #[implicit(8)]
    vendor_info: Option<&'a [u8]>,
    #[implicit(9)]
    tcb_type: Option<&'a [u8]>,
}

fn dice_tcb_flags_bitstring(flags: &template::Flags) -> asn1::OwnedBitString {
    let mut val = 0u8;
    if flags.not_configured {
        val |= 1 << 7;
    }
    if flags.not_secure {
        val |= 1 << 6;
    }
    if flags.recovery {
        val |= 1 << 5;
    }
    if flags.debug {
        val |= 1 << 4;
    }
    asn1::OwnedBitString::new(vec![val], 4).expect("cannot create an OwnedBitString for flags")
}

fn dice_tcb_info_extension(dice_tcb_info: &DiceTcbInfo) -> Result<X509Extension> {
    let der = asn1::write_single(dice_tcb_info)?;
    let octet_string = Asn1OctetString::new_from_bytes(&der)?;
    // Unfortunately, the rust binding does not seem to allow creating a Asn1Object
    // from a Nid so we have to manually create it from the OID string.
    let oid = Asn1Object::from_str("2.23.133.5.4.1")?;
    // From DICE specification:
    // The DiceTcbInfo extension SHOULD be marked critical.
    Ok(X509Extension::new_from_der(&oid, true, &octet_string)?)
}

impl template::AttributeType {
    fn nid(&self) -> Nid {
        match self {
            Self::Country => Nid::COUNTRYNAME,
            Self::Organization => Nid::ORGANIZATIONNAME,
            Self::OrganizationalUnit => Nid::ORGANIZATIONALUNITNAME,
            Self::State => Nid::STATEORPROVINCENAME,
            Self::CommonName => Nid::COMMONNAME,
            Self::SerialNumber => Nid::SERIALNUMBER,
        }
    }
}

impl template::HashAlgorithm {
    // Return the OID of the hash algorithm.
    fn oid(&self) -> asn1::ObjectIdentifier {
        match self {
            // From https://www.rfc-editor.org/rfc/rfc3560.html#appendix-A
            Self::Sha256 => asn1::ObjectIdentifier::from_string("2.16.840.1.101.3.4.2.1").unwrap(),
        }
    }

    // Return the size of the digest.
    fn digest_size(&self) -> usize {
        match self {
            Self::Sha256 => 20,
        }
    }
}

fn extract_signature(
    generator: &mut OffsetGenerator,
    signature: &template::Signature,
    sigder: &[u8],
) -> Result<()> {
    match signature {
        template::Signature::EcdsaWithSha256 { value } => {
            let ecdsa_sig = EcdsaSig::from_der(sigder)
                .context("cannot extract ECDSA signature from certificate")?;
            // The ASN1 representation of r and s are as big-endian integers which is what is returned by to_vec.
            let r = ecdsa_sig.r().to_vec();
            let s = ecdsa_sig.s().to_vec();
            // If the template does not specify a value then add hidden variables to clear them.
            if let Some(value) = value {
                // We only support variables and not literals.
                if value.r.is_literal() || value.s.is_literal() {
                    bail!("The generator only supports ecdsa signature templates where 'r' and 's' are variables, not a literals");
                }
                let target_type = template::VariableType::Integer {
                    size: r.len(),
                    endianness: template::Endianness::Big,
                    limb_size: 1,
                };
                // FIXME should probably check that conversion makes sense here.
                generator.add_variable(r, target_type, &value.r);
                generator.add_variable(s, target_type, &value.s);
            } else {
                generator.add_hidden_variable("__hidden_sig_ec_r".to_string(), r);
                generator.add_hidden_variable("__hidden_sig_ec_s".to_string(), s);
            }
            Ok(())
        }
    }
}

// Annoyingly, asn1 only has BigInt and not a owned version.
struct Asn1OwnedBigInt {
    data: Vec<u8>,
}

impl Asn1OwnedBigInt {
    fn from_biguint(biguint: &BigUint) -> Self {
        // There is a small annoyance here: the asn1 library expects the integer to be minimal
        // and also that if the top most significant bit is set, to prepend a zero byte to avoid
        // ambiguity in the DER encoding.
        let mut data = biguint.to_bytes_le();
        // Remove the MSB until it is not zero. Make sure to never remove all zeros: a valid
        // ASN1 integer must contain at least one byte.
        while data.len() >= 2 && *data.last().unwrap() == 0 {
            data.pop();
        }
        // If the MSB has its its most significant bit set, add a 0 byte.
        if data.last().unwrap().leading_zeros() == 0 {
            data.push(0);
        }
        // The ASN1 representation is in big endian so reverse the data.
        data.reverse();
        Asn1OwnedBigInt { data }
    }

    fn to_asn1_bigint(&self) -> asn1::BigInt {
        asn1::BigInt::new(&self.data).expect("asn1::BigInt should never fail in from_biguint")
    }
}

/// Generate an X509 certificate using a template certificate and an assignment
/// of the variables to their specific values.
pub fn generate_certificate(tmpl: &template::Template) -> Result<CertificateWithVariables> {
    let mut generator = OffsetGenerator::new(&tmpl.variables);

    let mut builder = X509::builder()?;

    // Certificate expiration time is fixed by the DICE
    // specification. TODO add ref
    let not_before = Asn1Time::from_str("20230101000000Z")?;
    builder.set_not_before(&not_before)?;
    let not_after = Asn1Time::from_str("99991231235959Z")?;
    builder.set_not_after(&not_after)?;

    // Fixed by spec. TODO add ref
    builder.set_version(3)?;

    // Serial number.
    let serial_number = generator
        .get_value(&tmpl.certificate.serial_number)
        .expect("cannot get value for the certificate serial number");
    let serial_number_asn1 = Asn1Integer::from_bn(&bignum_from_bigint(&serial_number))?;
    builder.set_serial_number(&serial_number_asn1)?;

    // Issuer.
    let mut issuer_name_builder = X509NameBuilder::new()?;
    for (key, value) in &tmpl.certificate.issuer {
        let value = generator.get_value(value).with_context(|| {
            format!(
                "cannot get value of key '{}' in the certificate issuer",
                key
            )
        })?;
        issuer_name_builder.append_entry_by_nid(key.nid(), &value)?;
    }
    let issuer_name = issuer_name_builder.build();
    builder.set_issuer_name(&issuer_name)?;

    // Subject.
    let mut subject_builder = X509NameBuilder::new()?;
    for (key, value) in &tmpl.certificate.subject {
        let value = generator.get_value(value).with_context(|| {
            format!(
                "cannot get value of key '{}' in the certificate subject",
                key
            )
        })?;
        subject_builder.append_entry_by_nid(key.nid(), &value)?;
    }
    let subject = subject_builder.build();
    builder.set_subject_name(&subject)?;

    // Standard extensions. Fixed by spec. TODO add ref
    builder.append_extension(extension::BasicConstraints::new().critical().ca().build()?)?;

    builder.append_extension(
        extension::KeyUsage::new()
            .critical()
            .key_cert_sign()
            .build()?,
    )?;

    // The rust openssl binding does not allow to choose the subject key ID
    // and always defaults to the "hash" method of the standard. We need to use
    // raw ASN1 to work around this.
    let subject_key_id = generator
        .get_value(&tmpl.certificate.subject_key_identifier)
        .context("cannot get value for the certificate subject key ID")?;
    builder.append_extension(subject_key_id_extension(&subject_key_id)?)?;

    // Openssl does not support creating an auth key identifier extension without a CA
    // or without some low-level fiddling. We need to use raw ASN1 for that.
    let auth_key_identifier = generator.get_value(&tmpl.certificate.authority_key_identifier)?;
    builder.append_extension(auth_key_id_extension(&auth_key_identifier)?)?;

    // Collect firmware IDs.
    let fwids_digests: Vec<Vec<u8>> = tmpl
        .certificate
        .fw_ids
        .as_ref()
        .unwrap_or(&vec![])
        .iter()
        .enumerate()
        .map(|(i, fwid)| {
            let digest = generator.get_value(&fwid.digest).with_context(|| {
                format!("cannot get value for the certificate fw_id entry {}", i + 1)
            })?;
            if digest.len() != fwid.hash_algorithm.digest_size() {
                bail!(
                    "hash algorithm {:?} has digest size {} but the specified digest has size {}",
                    fwid.hash_algorithm,
                    fwid.hash_algorithm.digest_size(),
                    digest.len()
                );
            }
            Ok(digest)
        })
        .collect::<Result<Vec<_>>>()?;

    let fwids = tmpl.certificate.fw_ids.as_ref().map(|fw_ids| {
        let mut fwids = Vec::new();
        for (i, fwid) in fw_ids.iter().enumerate() {
            fwids.push(Fwid {
                hash_alg: fwid.hash_algorithm.oid(),
                digest: &fwids_digests[i],
            });
        }
        fwids
    });

    // DICE TcbInfo
    let vendor = tmpl
        .certificate
        .vendor
        .as_ref()
        .map(|vendor| {
            generator
                .get_value(vendor)
                .context("cannot get value for the certificate DICE vendor")
        })
        .transpose()?;
    let model = tmpl
        .certificate
        .model
        .as_ref()
        .map(|model| {
            generator
                .get_value(model)
                .context("cannot get value for the certificate DICE model")
        })
        .transpose()?;
    let version = tmpl
        .certificate
        .version
        .as_ref()
        .map(|ver| {
            generator
                .get_value(ver)
                .context("cannot get value for the certificate DICE version")
        })
        .transpose()?;
    let svn = tmpl
        .certificate
        .svn
        .as_ref()
        .map(|svn| {
            generator
                .get_value(svn)
                .context("cannot get value for the certificate DICE svn")
        })
        .transpose()?
        .as_ref()
        .map(Asn1OwnedBigInt::from_biguint);
    let layer = tmpl
        .certificate
        .layer
        .as_ref()
        .map(|layer| {
            generator
                .get_value(layer)
                .context("cannot get value for the certificate DICE svn")
        })
        .transpose()?
        .as_ref()
        .map(Asn1OwnedBigInt::from_biguint);
    let flags = tmpl
        .certificate
        .flags
        .as_ref()
        .map(dice_tcb_flags_bitstring);
    let dice_tcb_info = DiceTcbInfo {
        vendor: vendor.as_deref().map(asn1::Utf8String::new),
        model: model.as_deref().map(asn1::Utf8String::new),
        version: version.as_deref().map(asn1::Utf8String::new),
        svn: svn.as_ref().map(Asn1OwnedBigInt::to_asn1_bigint),
        index: None,
        layer: layer.as_ref().map(Asn1OwnedBigInt::to_asn1_bigint),
        fwids: fwids.as_deref().map(asn1::SequenceOfWriter::new),
        flags: flags.as_ref().map(asn1::OwnedBitString::as_bitstring),
        vendor_info: None,
        tcb_type: None,
    };
    builder.append_extension(dice_tcb_info_extension(&dice_tcb_info)?)?;

    // Random public key.
    let pubkey = get_pubkey(&mut generator, &tmpl.certificate.subject_public_key_info)?;
    builder.set_pubkey(&pubkey)?;

    // Sign the certificate with a random public key.
    let algo = get_signature_algorithm(&tmpl.certificate.signature)?;
    builder.sign(&algo.key_pair, algo.hash)?;

    let x509 = builder.build();
    log::info!("Certificate: {:#?}", x509);

    // At this point, we need to find out the value of the signature so
    // we can get the offset. This is made difficult by the fact that
    // the signature is a bit string that itself contains a DER-encoded
    // algorithm-specific structure.
    let signature = x509.signature();
    extract_signature(
        &mut generator,
        &tmpl.certificate.signature,
        signature.as_slice(),
    )?;

    let der = x509.to_der()?;
    // FIXME: clearing the fields makes OpenSSL fail to parse the certificate.
    generator.generate(tmpl.name.clone(), der, false)
}
