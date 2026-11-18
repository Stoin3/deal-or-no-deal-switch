# -*- coding: utf-8 -*-
"""Synthesises every presenter line with Piper TTS into romfs/voice/<lang>/*.ogg
and writes romfs/voice/index.txt (group, variant, dutch subtitle, english subtitle).
Re-runs only synthesise lines whose text changed."""
import hashlib, io, json, os, sys, wave
import numpy as np
import soundfile as sf
from piper import PiperVoice, SynthesisConfig

sys.path.insert(0, os.path.dirname(__file__))
import lines

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT = os.path.join(ROOT, "romfs", "voice")
VOICES = {
    "nl": os.path.join(ROOT, "tools", "voices", os.environ.get("NL_VOICE", "nl_NL-pim-medium") + ".onnx"),
    "en": os.path.join(ROOT, "tools", "voices", os.environ.get("EN_VOICE", "en_US-ryan-high") + ".onnx"),
}
CACHE = os.path.join(ROOT, "tools", ".voice_cache.json")


def synth(voice, text, cfg):
    buf = io.BytesIO()
    with wave.open(buf, "wb") as w:
        voice.synthesize_wav(text, w, cfg)
    buf.seek(0)
    with wave.open(buf, "rb") as w:
        sr = w.getframerate()
        data = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float32) / 32768.0
    # trim silence
    thr = 0.01
    idx = np.where(np.abs(data) > thr)[0]
    if len(idx):
        a = max(0, idx[0] - int(0.02 * sr)); b = min(len(data), idx[-1] + int(0.06 * sr))
        data = data[a:b]
    peak = np.max(np.abs(data)) or 1.0
    data = data / peak * 0.89
    return data, sr


def write_ogg(path, data, sr):
    with sf.SoundFile(path, "w", sr, 1, format="OGG", subtype="VORBIS") as f:
        for i in range(0, len(data), 4096):
            f.write(data[i:i + 4096])


def main():
    cache = json.load(open(CACHE)) if os.path.exists(CACHE) else {}
    loaded = {}
    index_lines = []
    total = sum(len(v) for v in lines.G.values())
    done = 0
    for group, variants in lines.G.items():
        for vi, pair in enumerate(variants):
            sub = lines.subtitle_for(group, pair)
            index_lines.append(f"{group}\t{vi}\t{sub[0]}\t{sub[1]}")
            for li, lang in enumerate(("nl", "en")):
                text = pair[li]
                is_amount = group.startswith("amt_")
                key = f"{lang}/{group}__{vi}"
                h = hashlib.md5((text + VOICES[lang] + "v3").encode("utf-8")).hexdigest()
                path = os.path.join(OUT, lang, f"{group}__{vi}.ogg")
                if cache.get(key) == h and os.path.exists(path):
                    continue
                if lang not in loaded:
                    loaded[lang] = PiperVoice.load(VOICES[lang])
                cfg = SynthesisConfig(length_scale=0.97 if is_amount else 0.9, noise_scale=0.75, noise_w_scale=0.9)
                data, sr = synth(loaded[lang], text, cfg)
                os.makedirs(os.path.dirname(path), exist_ok=True)
                write_ogg(path, data.astype(np.float32), sr)
                cache[key] = h
            done += 1
            if done % 50 == 0:
                print(f"{done}/{total}", flush=True)
                json.dump(cache, open(CACHE, "w"))
    json.dump(cache, open(CACHE, "w"))
    with open(os.path.join(OUT, "index.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(index_lines) + "\n")
    print("done", total)


if __name__ == "__main__":
    main()
