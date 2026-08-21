#!/usr/bin/env python3
"""Local report-only camera worker. Frames never leave this host."""
import argparse, json, time, urllib.request
from pathlib import Path

def jpeg_frames(url, timeout=5):
    req = urllib.request.Request(url, headers={"User-Agent": "PrinterHMI-host-camera-ai/0.1"})
    with urllib.request.urlopen(req, timeout=timeout) as stream:
        buf = bytearray()
        while True:
            chunk = stream.read(8192)
            if not chunk: return
            buf.extend(chunk)
            while True:
                start = buf.find(b"\xff\xd8")
                end = buf.find(b"\xff\xd9", start + 2)
                if start < 0 or end < 0: break
                yield bytes(buf[start:end + 2]); del buf[:end + 2]

def infer(frame, model, labels, check, failure):
    if not model: return "check", 0.0, "model-not-configured"
    try:
        import numpy as np
        from PIL import Image
        import onnxruntime as ort
        image = Image.open(__import__('io').BytesIO(frame)).convert("RGB").resize((224, 224))
        tensor = np.asarray(image, dtype=np.float32) / 255.0
        tensor = np.transpose(tensor, (2, 0, 1))[None, ...]
        session = model if not isinstance(model, str) else ort.InferenceSession(model, providers=["CPUExecutionProvider"])
        output = session.run(None, {session.get_inputs()[0].name: tensor})[0][0]
        score = float(np.max(output)); index = int(np.argmax(output))
        label = labels[index] if index < len(labels) else "unknown"
        status = "failure" if score >= failure else ("check" if score >= check else "normal")
        return status, score, label
    except Exception as exc:
        return "check", 0.0, "inference-error:" + type(exc).__name__

def main():
    p = argparse.ArgumentParser(); p.add_argument("--camera-url", required=True)
    p.add_argument("--model"); p.add_argument("--state-file", default="camera-ai-state.json")
    p.add_argument("--fps", type=float, default=2); p.add_argument("--check", type=float, default=.65)
    p.add_argument("--failure", type=float, default=.90); p.add_argument("--labels", default="normal,defect")
    a = p.parse_args(); labels = [x.strip() for x in a.labels.split(",")]; model = a.model
    last = 0.0
    try:
        for frame in jpeg_frames(a.camera_url):
            now = time.time()
            if now - last < 1.0 / max(a.fps, .1): continue
            last = now; status, confidence, defect = infer(frame, model, labels, a.check, a.failure)
            state = {"status": status, "confidence": round(confidence, 4), "defect": defect,
                     "updated": int(now), "source": "host-camera-ai"}
            Path(a.state_file).write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")
            print(json.dumps(state), flush=True)
    except Exception as exc:
        state = {"status": "offline", "confidence": 0.0, "defect": type(exc).__name__,
                 "updated": int(time.time()), "source": "host-camera-ai"}
        Path(a.state_file).write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(state), flush=True); return 1

if __name__ == "__main__": raise SystemExit(main())
