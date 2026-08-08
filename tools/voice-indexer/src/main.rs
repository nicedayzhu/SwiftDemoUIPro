use anyhow::{bail, Context as _, Result};
use source2_demo::prelude::{
    Context, DemoRunner, Interests, Observer, ObserverResult, Parser, SvcMessages,
};
use source2_demo::proto::{CSvcMsgVoiceData, Message};
use std::collections::BTreeMap;
use std::env;
use std::fs::{self, File, OpenOptions};
use std::io::{self, Read, Write};
use std::path::{Path, PathBuf};

const SOURCE2_DEMO_MAGIC: &[u8; 8] = b"PBDEMS2\0";
const MAX_DECOMPRESSED_DEMO_SIZE: u64 = 8 * 1024 * 1024 * 1024;
const MAX_PLAYER_ENTITY_INDEX: i32 = 64;
const SPEAKING_HOLD_TICKS: u32 = 30;
const SOURCE2_VJS_HEADER_VERSION: u32 = 0x0004_000c;
const SOURCE2_RESOURCE_VERSION: u32 = 8;
const VPK_SIGNATURE: u32 = 0x55aa_1234;
const VPK_VERSION: u32 = 1;
const VPK_EMBEDDED_ARCHIVE_INDEX: u16 = 0x7fff;
const VPK_ENTRY_TERMINATOR: u16 = 0xffff;

#[derive(Debug, Default)]
struct VoiceIndex {
    voice_packets: u64,
    malformed_packets: u64,
    unresolved_packets: u64,
    first_tick: Option<u32>,
    last_tick: Option<u32>,
    pulses_by_slot: BTreeMap<i32, Vec<u32>>,
}

impl VoiceIndex {
    fn record_voice(&mut self, slot: i32, tick: u32) {
        let pulses = self.pulses_by_slot.entry(slot).or_default();
        if pulses.last().copied() != Some(tick) {
            pulses.push(tick);
        }
    }

    fn pulse_count(&self) -> usize {
        self.pulses_by_slot.values().map(Vec::len).sum()
    }
}

#[derive(Default)]
struct VoiceIndexCollector {
    index: VoiceIndex,
}

impl Observer for VoiceIndexCollector {
    fn interests(&self) -> Interests {
        Interests::SVC_MESSAGE
    }

    fn on_svc_message(
        &mut self,
        ctx: &Context,
        msg_type: SvcMessages,
        message: &[u8],
    ) -> ObserverResult {
        if msg_type != SvcMessages::SvcVoiceData {
            return Ok(());
        }

        let tick = ctx.tick();
        self.index.voice_packets += 1;
        self.index.first_tick = Some(self.index.first_tick.map_or(tick, |first| first.min(tick)));
        self.index.last_tick = Some(self.index.last_tick.map_or(tick, |last| last.max(tick)));

        let voice = match CSvcMsgVoiceData::decode(message) {
            Ok(voice) => voice,
            Err(_) => {
                self.index.malformed_packets += 1;
                return Ok(());
            }
        };
        let Some(entity_index) = resolve_speaker_entity(&voice) else {
            self.index.unresolved_packets += 1;
            return Ok(());
        };
        self.index.record_voice(entity_index - 1, tick);
        Ok(())
    }
}

fn resolve_speaker_entity(voice: &CSvcMsgVoiceData) -> Option<i32> {
    voice
        .entity
        .filter(|entity| (1..=MAX_PLAYER_ENTITY_INDEX).contains(entity))
        .or_else(|| {
            voice
                .client_deprecated
                .map(|client| client + 1)
                .filter(|entity| (1..=MAX_PLAYER_ENTITY_INDEX).contains(entity))
        })
}

fn parse_demo(path: &Path) -> Result<VoiceIndex> {
    let input =
        File::open(path).with_context(|| format!("failed to open demo: {}", path.display()))?;
    let mut parser = Parser::from_reader(input)?;
    let collector = parser.register_observer::<VoiceIndexCollector>();
    parser.run_to_end()?;

    let collector = collector.borrow();
    Ok(VoiceIndex {
        voice_packets: collector.index.voice_packets,
        malformed_packets: collector.index.malformed_packets,
        unresolved_packets: collector.index.unresolved_packets,
        first_tick: collector.index.first_tick,
        last_tick: collector.index.last_tick,
        pulses_by_slot: collector.index.pulses_by_slot.clone(),
    })
}

fn render_panorama_source(index: &VoiceIndex) -> String {
    let mut source = String::from(
        "\"use strict\";var SwiftDemoVoiceData={\"schemaVersion\":1,\"generated\":true,\"holdTicks\":",
    );
    source.push_str(&SPEAKING_HOLD_TICKS.to_string());
    source.push_str(",\"voicePacketCount\":");
    source.push_str(&index.voice_packets.to_string());
    source.push_str(",\"malformedPacketCount\":");
    source.push_str(&index.malformed_packets.to_string());
    source.push_str(",\"unresolvedPacketCount\":");
    source.push_str(&index.unresolved_packets.to_string());
    source.push_str(",\"firstTick\":");
    source.push_str(&index.first_tick.unwrap_or(0).to_string());
    source.push_str(",\"lastTick\":");
    source.push_str(&index.last_tick.unwrap_or(0).to_string());
    source.push_str(",\"pulsesBySlot\":{");

    for (slot_index, (slot, ticks)) in index.pulses_by_slot.iter().enumerate() {
        if slot_index > 0 {
            source.push(',');
        }
        source.push('"');
        source.push_str(&slot.to_string());
        source.push_str("\":[");
        for (tick_index, tick) in ticks.iter().enumerate() {
            if tick_index > 0 {
                source.push(',');
            }
            source.push_str(&tick.to_string());
        }
        source.push(']');
    }
    source.push_str("}};");
    source
}

fn temporary_output_path(output_path: &Path) -> PathBuf {
    let file_name = output_path
        .file_name()
        .and_then(|name| name.to_str())
        .unwrap_or("swift_demo_voice_data.vjs");
    output_path.with_file_name(format!("{file_name}.part"))
}

fn write_output_bytes(output_path: &Path, bytes: &[u8]) -> Result<()> {
    if let Some(parent) = output_path.parent() {
        fs::create_dir_all(parent)
            .with_context(|| format!("failed to create output directory: {}", parent.display()))?;
    }
    let temporary_path = temporary_output_path(output_path);
    if temporary_path.exists() {
        fs::remove_file(&temporary_path).with_context(|| {
            format!(
                "failed to remove stale temporary output: {}",
                temporary_path.display()
            )
        })?;
    }
    let mut output = OpenOptions::new()
        .write(true)
        .create_new(true)
        .open(&temporary_path)
        .with_context(|| format!("failed to create output: {}", temporary_path.display()))?;
    output.write_all(bytes)?;
    output.sync_all()?;
    drop(output);

    if output_path.exists() {
        fs::remove_file(output_path)
            .with_context(|| format!("failed to replace output: {}", output_path.display()))?;
    }
    fs::rename(&temporary_path, output_path).with_context(|| {
        format!(
            "failed to publish {} as {}",
            temporary_path.display(),
            output_path.display()
        )
    })?;
    Ok(())
}

fn write_output(output_path: &Path, source: &str) -> Result<()> {
    write_output_bytes(output_path, source.as_bytes())
}

fn unpack_zstd_demo(input_path: &Path, output_path: &Path) -> Result<()> {
    if !input_path.is_file() {
        bail!(
            "Zstandard-compressed demo does not exist: {}",
            input_path.display()
        );
    }
    if input_path == output_path {
        bail!("input and output paths must differ");
    }
    if let Some(parent) = output_path.parent() {
        fs::create_dir_all(parent)
            .with_context(|| format!("failed to create output directory: {}", parent.display()))?;
    }

    let temporary_path = temporary_output_path(output_path);
    if temporary_path.exists() {
        fs::remove_file(&temporary_path).with_context(|| {
            format!(
                "failed to remove stale temporary output: {}",
                temporary_path.display()
            )
        })?;
    }

    let unpacked = (|| -> Result<u64> {
        let input = File::open(input_path).with_context(|| {
            format!(
                "failed to open Zstandard-compressed demo: {}",
                input_path.display()
            )
        })?;
        let mut decoder = zstd::stream::read::Decoder::new(input)
            .context("failed to initialize the Zstandard decoder")?;
        let mut output = OpenOptions::new()
            .write(true)
            .create_new(true)
            .open(&temporary_path)
            .with_context(|| format!("failed to create output: {}", temporary_path.display()))?;

        let mut magic = [0u8; SOURCE2_DEMO_MAGIC.len()];
        decoder
            .read_exact(&mut magic)
            .context("the decompressed file is too short to be a CS2 demo")?;
        if &magic != SOURCE2_DEMO_MAGIC {
            bail!("the Zstandard payload is not a CS2 demo (PBDEMS2 header missing)");
        }
        output.write_all(&magic)?;

        let remaining_limit = MAX_DECOMPRESSED_DEMO_SIZE - SOURCE2_DEMO_MAGIC.len() as u64;
        let copied = io::copy(&mut decoder.take(remaining_limit + 1), &mut output)
            .context("failed while decompressing the Zstandard demo")?;
        if copied > remaining_limit {
            bail!("the decompressed demo exceeds the 8 GB safety limit");
        }
        output.sync_all()?;
        Ok(copied + SOURCE2_DEMO_MAGIC.len() as u64)
    })();

    let unpacked_bytes = match unpacked {
        Ok(bytes) => bytes,
        Err(error) => {
            let _ = fs::remove_file(&temporary_path);
            return Err(error);
        }
    };

    if output_path.exists() {
        fs::remove_file(output_path)
            .with_context(|| format!("failed to replace output: {}", output_path.display()))?;
    }
    fs::rename(&temporary_path, output_path).with_context(|| {
        format!(
            "failed to publish {} as {}",
            temporary_path.display(),
            output_path.display()
        )
    })?;

    println!("Zstandard demo ready");
    println!("  decompressed bytes: {unpacked_bytes}");
    Ok(())
}

fn crc32(bytes: &[u8]) -> u32 {
    let mut crc = 0xffff_ffffu32;
    for byte in bytes {
        crc ^= u32::from(*byte);
        for _ in 0..8 {
            let mask = 0u32.wrapping_sub(crc & 1);
            crc = (crc >> 1) ^ (0xedb8_8320 & mask);
        }
    }
    !crc
}

fn push_c_string(output: &mut Vec<u8>, value: &str) {
    output.extend_from_slice(value.as_bytes());
    output.push(0);
}

fn build_panorama_vjs_resource(source: &[u8]) -> Result<Vec<u8>> {
    let source_length = u32::try_from(source.len())
        .context("Panorama voice index is too large for a Source 2 resource")?;

    // This is the minimal vjs [Version 4] resource emitted by PanoramaCompiler:
    // a Source 2 resource header followed by an empty RED2 block and the UTF-8
    // JavaScript in DATA. Both blocks begin at the same 16-byte-aligned offset
    // because RED2 is empty.
    let mut resource = Vec::with_capacity(48 + source.len());
    resource.extend_from_slice(&0u32.to_le_bytes());
    resource.extend_from_slice(&SOURCE2_VJS_HEADER_VERSION.to_le_bytes());
    resource.extend_from_slice(&SOURCE2_RESOURCE_VERSION.to_le_bytes());
    resource.extend_from_slice(&2u32.to_le_bytes());

    resource.extend_from_slice(b"RED2");
    resource.extend_from_slice(&28u32.to_le_bytes());
    resource.extend_from_slice(&0u32.to_le_bytes());

    resource.extend_from_slice(b"DATA");
    resource.extend_from_slice(&16u32.to_le_bytes());
    resource.extend_from_slice(&source_length.to_le_bytes());

    resource.resize(48, 0);
    resource.extend_from_slice(source);
    let file_size =
        u32::try_from(resource.len()).context("compiled Panorama voice resource is too large")?;
    resource[0..4].copy_from_slice(&file_size.to_le_bytes());
    Ok(resource)
}

fn build_session_vpk(resource: &[u8]) -> Result<Vec<u8>> {
    let resource_length = u32::try_from(resource.len())
        .context("compiled voice resource is too large for a VPK entry")?;
    let mut tree = Vec::new();
    push_c_string(&mut tree, "vjs_c");
    push_c_string(&mut tree, "panorama/scripts/hud");
    push_c_string(&mut tree, "swift_demo_voice_data");
    tree.extend_from_slice(&crc32(resource).to_le_bytes());
    tree.extend_from_slice(&0u16.to_le_bytes());
    tree.extend_from_slice(&VPK_EMBEDDED_ARCHIVE_INDEX.to_le_bytes());
    tree.extend_from_slice(&0u32.to_le_bytes());
    tree.extend_from_slice(&resource_length.to_le_bytes());
    tree.extend_from_slice(&VPK_ENTRY_TERMINATOR.to_le_bytes());
    tree.extend_from_slice(&[0, 0, 0]);

    let tree_length = u32::try_from(tree.len()).context("VPK directory tree is too large")?;
    let mut vpk = Vec::with_capacity(12 + tree.len() + resource.len());
    vpk.extend_from_slice(&VPK_SIGNATURE.to_le_bytes());
    vpk.extend_from_slice(&VPK_VERSION.to_le_bytes());
    vpk.extend_from_slice(&tree_length.to_le_bytes());
    vpk.extend_from_slice(&tree);
    vpk.extend_from_slice(resource);
    Ok(vpk)
}

fn pack_session_vpk(input_path: &Path, output_path: &Path) -> Result<()> {
    if !input_path.is_file() {
        bail!(
            "compiled voice resource does not exist: {}",
            input_path.display()
        );
    }
    if input_path == output_path {
        bail!("input and output paths must differ");
    }
    let resource = fs::read(input_path).with_context(|| {
        format!(
            "failed to read compiled voice resource: {}",
            input_path.display()
        )
    })?;
    let vpk = build_session_vpk(&resource)?;
    write_output_bytes(output_path, &vpk)?;
    println!("voice session VPK ready");
    println!("  resource bytes: {}", resource.len());
    println!("  VPK bytes: {}", vpk.len());
    Ok(())
}

fn trim_ascii_whitespace(bytes: &[u8]) -> &[u8] {
    let start = bytes
        .iter()
        .position(|byte| !byte.is_ascii_whitespace())
        .unwrap_or(bytes.len());
    let end = bytes
        .iter()
        .rposition(|byte| !byte.is_ascii_whitespace())
        .map_or(start, |index| index + 1);
    &bytes[start..end]
}

fn compile_panorama_vjs(input_path: &Path, output_path: &Path) -> Result<()> {
    if !input_path.is_file() {
        bail!(
            "Panorama JavaScript input does not exist: {}",
            input_path.display()
        );
    }
    if input_path == output_path {
        bail!("input and output paths must differ");
    }

    let source = fs::read(input_path).with_context(|| {
        format!(
            "failed to read Panorama JavaScript: {}",
            input_path.display()
        )
    })?;
    let source = source.strip_prefix(&[0xef, 0xbb, 0xbf]).unwrap_or(&source);
    let source = trim_ascii_whitespace(source);
    let resource = build_panorama_vjs_resource(source)?;
    write_output_bytes(output_path, &resource)?;
    println!("compiled Panorama vjs resource ready");
    println!("  source bytes: {}", source.len());
    println!("  resource bytes: {}", resource.len());
    Ok(())
}

fn print_voice_index_summary(index: &VoiceIndex) {
    println!("  voice packets: {}", index.voice_packets);
    println!("  display pulses: {}", index.pulse_count());
    println!("  speaker slots: {}", index.pulses_by_slot.len());
    println!(
        "  tick range: {:?}..={:?}",
        index.first_tick, index.last_tick
    );
    println!("  malformed packets: {}", index.malformed_packets);
    println!("  unresolved packets: {}", index.unresolved_packets);
}

fn build_voice_index(input_path: &Path, output_path: &Path) -> Result<()> {
    if !input_path.is_file() {
        bail!("input demo does not exist: {}", input_path.display());
    }
    if input_path == output_path {
        bail!("input and output paths must differ");
    }

    let index = parse_demo(input_path)?;
    let source = render_panorama_source(&index);
    write_output(output_path, &source)?;

    println!("voice index ready");
    print_voice_index_summary(&index);
    Ok(())
}

fn build_voice_session(input_path: &Path, output_path: &Path) -> Result<()> {
    if !input_path.is_file() {
        bail!("input demo does not exist: {}", input_path.display());
    }
    if input_path == output_path {
        bail!("input and output paths must differ");
    }

    let index = parse_demo(input_path)?;
    let source = render_panorama_source(&index);
    let resource = build_panorama_vjs_resource(source.as_bytes())?;
    let vpk = build_session_vpk(&resource)?;
    write_output_bytes(output_path, &vpk)?;

    println!("voice session VPK ready");
    print_voice_index_summary(&index);
    println!("  source bytes: {}", source.len());
    println!("  resource bytes: {}", resource.len());
    println!("  VPK bytes: {}", vpk.len());
    Ok(())
}

fn run() -> Result<()> {
    let arguments: Vec<String> = env::args().collect();
    match arguments.as_slice() {
        [_, input, output] => build_voice_index(Path::new(input), Path::new(output)),
        [_, command, input, output] if command == "pack-vpk" => {
            pack_session_vpk(Path::new(input), Path::new(output))
        }
        [_, command, input, output] if command == "compile-vjs" => {
            compile_panorama_vjs(Path::new(input), Path::new(output))
        }
        [_, command, input, output] if command == "build-session-vpk" => {
            build_voice_session(Path::new(input), Path::new(output))
        }
        [_, command, input, output] if command == "unpack-zst" => {
            unpack_zstd_demo(Path::new(input), Path::new(output))
        }
        _ => bail!(
            "usage: swift-demo-voice-indexer <input.dem> <output.vjs>\n       swift-demo-voice-indexer compile-vjs <input.vjs> <output.vjs_c>\n       swift-demo-voice-indexer pack-vpk <input.vjs_c> <output.vpk>\n       swift-demo-voice-indexer build-session-vpk <input.dem> <output.vpk>\n       swift-demo-voice-indexer unpack-zst <input.dem.zst> <output.dem>"
        ),
    }
}

fn main() -> Result<()> {
    std::thread::Builder::new()
        .name("swift-demo-voice-indexer".to_string())
        .stack_size(32 * 1024 * 1024)
        .spawn(run)?
        .join()
        .map_err(|_| anyhow::anyhow!("voice indexer worker thread panicked"))?
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::{SystemTime, UNIX_EPOCH};

    fn unique_test_directory(name: &str) -> PathBuf {
        let nonce = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("system clock should be after the Unix epoch")
            .as_nanos();
        env::temp_dir().join(format!(
            "swift-demo-voice-indexer-{name}-{}-{nonce}",
            std::process::id()
        ))
    }

    #[test]
    fn resolves_current_and_legacy_speaker_fields() {
        let current = CSvcMsgVoiceData {
            entity: Some(7),
            client_deprecated: Some(42),
            ..CSvcMsgVoiceData::default()
        };
        assert_eq!(resolve_speaker_entity(&current), Some(7));

        let legacy = CSvcMsgVoiceData {
            entity: None,
            client_deprecated: Some(6),
            ..CSvcMsgVoiceData::default()
        };
        assert_eq!(resolve_speaker_entity(&legacy), Some(7));
    }

    #[test]
    fn deduplicates_multiple_voice_packets_in_one_tick() {
        let mut index = VoiceIndex::default();
        index.record_voice(6, 100);
        index.record_voice(6, 100);
        index.record_voice(6, 101);
        assert_eq!(index.pulses_by_slot.get(&6), Some(&vec![100, 101]));
    }

    #[test]
    fn renders_compact_ascii_panorama_data() {
        let mut index = VoiceIndex::default();
        index.voice_packets = 2;
        index.first_tick = Some(100);
        index.last_tick = Some(101);
        index.record_voice(6, 100);
        index.record_voice(6, 101);
        let source = render_panorama_source(&index);
        assert!(source.is_ascii());
        assert!(source.contains("var SwiftDemoVoiceData="));
        assert!(source.contains("\"generated\":true"));
        assert!(source.contains("\"6\":[100,101]"));
        assert!(source.ends_with("}};"));
    }

    #[test]
    fn builds_single_file_voice_session_vpk() {
        assert_eq!(crc32(b"123456789"), 0xcbf4_3926);
        let resource = b"compiled panorama voice data";
        let vpk = build_session_vpk(resource).expect("VPK should build");
        assert_eq!(
            u32::from_le_bytes(vpk[0..4].try_into().unwrap()),
            VPK_SIGNATURE
        );
        assert_eq!(
            u32::from_le_bytes(vpk[4..8].try_into().unwrap()),
            VPK_VERSION
        );
        let tree_length = u32::from_le_bytes(vpk[8..12].try_into().unwrap()) as usize;
        assert!(tree_length > 18);
        assert!(vpk[12..12 + tree_length]
            .windows(b"panorama/scripts/hud".len())
            .any(|window| window == b"panorama/scripts/hud"));
        assert_eq!(&vpk[12 + tree_length..], resource);
    }

    #[test]
    fn builds_minimal_panorama_vjs_version_4_resource() {
        let source = b"\"use strict\";\nvar SwiftDemoVoiceData={};\n";
        let resource = build_panorama_vjs_resource(source).expect("vjs resource should build");

        assert_eq!(
            u32::from_le_bytes(resource[0..4].try_into().unwrap()) as usize,
            resource.len()
        );
        assert_eq!(
            u32::from_le_bytes(resource[4..8].try_into().unwrap()),
            SOURCE2_VJS_HEADER_VERSION
        );
        assert_eq!(
            u32::from_le_bytes(resource[8..12].try_into().unwrap()),
            SOURCE2_RESOURCE_VERSION
        );
        assert_eq!(u32::from_le_bytes(resource[12..16].try_into().unwrap()), 2);
        assert_eq!(&resource[16..20], b"RED2");
        assert_eq!(u32::from_le_bytes(resource[20..24].try_into().unwrap()), 28);
        assert_eq!(u32::from_le_bytes(resource[24..28].try_into().unwrap()), 0);
        assert_eq!(&resource[28..32], b"DATA");
        assert_eq!(u32::from_le_bytes(resource[32..36].try_into().unwrap()), 16);
        assert_eq!(
            u32::from_le_bytes(resource[36..40].try_into().unwrap()) as usize,
            source.len()
        );
        assert!(resource[40..48].iter().all(|byte| *byte == 0));
        assert_eq!(&resource[48..], source);
    }

    #[test]
    fn packs_compiled_panorama_voice_resource() {
        let source = render_panorama_source(&VoiceIndex::default());
        let resource = build_panorama_vjs_resource(source.as_bytes()).unwrap();
        let vpk = build_session_vpk(&resource).unwrap();
        let tree_length = u32::from_le_bytes(vpk[8..12].try_into().unwrap()) as usize;
        let embedded = &vpk[12 + tree_length..];
        assert_eq!(embedded, resource);
        assert_eq!(
            u32::from_le_bytes(embedded[4..8].try_into().unwrap()),
            SOURCE2_VJS_HEADER_VERSION
        );
    }

    #[test]
    fn unpacks_zstandard_compressed_cs2_demo_atomically() {
        let directory = unique_test_directory("unpack-zst");
        fs::create_dir_all(&directory).unwrap();
        let input_path = directory.join("match.dem.zst");
        let output_path = directory.join("current.dem");
        let payload = [SOURCE2_DEMO_MAGIC.as_slice(), b"test-demo-payload"].concat();
        let compressed = zstd::stream::encode_all(payload.as_slice(), 1).unwrap();
        fs::write(&input_path, compressed).unwrap();

        unpack_zstd_demo(&input_path, &output_path).unwrap();

        assert_eq!(fs::read(&output_path).unwrap(), payload);
        assert!(!temporary_output_path(&output_path).exists());
        fs::remove_dir_all(directory).unwrap();
    }

    #[test]
    fn rejects_zstandard_payload_without_cs2_demo_header() {
        let directory = unique_test_directory("reject-zst");
        fs::create_dir_all(&directory).unwrap();
        let input_path = directory.join("not-a-demo.zst");
        let output_path = directory.join("current.dem");
        let compressed = zstd::stream::encode_all(b"not a cs2 demo".as_slice(), 1).unwrap();
        fs::write(&input_path, compressed).unwrap();

        let error = unpack_zstd_demo(&input_path, &output_path).unwrap_err();

        assert!(error.to_string().contains("PBDEMS2"));
        assert!(!output_path.exists());
        assert!(!temporary_output_path(&output_path).exists());
        fs::remove_dir_all(directory).unwrap();
    }
}
