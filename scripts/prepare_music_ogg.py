#!/usr/bin/env python3
"""Create compact OGG/Vorbis sidecars from the original Zombie Shooter M4A files.

The original .m4a files are never modified or deleted.  Output is fixed at
44.1 kHz stereo because the Vita mixer path intentionally has no resampler.
Requires an ffmpeg build with the libvorbis encoder.
"""
import argparse
import hashlib
import json
import subprocess
import zipfile
from pathlib import Path

NAMES=("menu_mus01","mus01","mus02","amb01","rain")


def probe(ffprobe: str, path: Path) -> dict:
    raw=subprocess.check_output([
        ffprobe,"-v","error","-select_streams","a:0",
        "-show_entries","stream=codec_name,sample_rate,channels,duration,bit_rate",
        "-of","json",str(path)
    ])
    data=json.loads(raw)
    if not data.get("streams"):
        raise RuntimeError(f"No audio stream: {path}")
    return data["streams"][0]


def main() -> None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--data",type=Path,default=Path("data"),
                   help="Zombie Shooter data root containing assets/music")
    p.add_argument("--ffmpeg",default="ffmpeg")
    p.add_argument("--ffprobe",default="ffprobe")
    p.add_argument("--quality",type=float,default=5.0,
                   help="libvorbis VBR quality (default 5, roughly 160 kbps)")
    p.add_argument("--output",type=Path,
                   default=Path("build-session-debug/music-ogg.zip"))
    a=p.parse_args()

    music=a.data.resolve()/"assets/music"
    records=[]
    for name in NAMES:
        src=music/(name+".m4a")
        dst=music/(name+".ogg")
        tmp=music/(name+".encode.tmp.ogg")
        if not src.is_file():
            p.error("Missing original: "+str(src))
        src_hash=hashlib.sha256(src.read_bytes()).hexdigest()
        subprocess.run([
            a.ffmpeg,"-v","error","-nostdin","-y","-i",str(src),
            "-map","0:a:0","-vn","-c:a","libvorbis","-q:a",str(a.quality),
            "-ar","44100","-ac","2",str(tmp)
        ],check=True)
        info=probe(a.ffprobe,tmp)
        if info.get("codec_name")!="vorbis" or int(info.get("sample_rate",0))!=44100 or int(info.get("channels",0))!=2:
            raise RuntimeError(f"Unexpected OGG output for {name}: {info}")
        if hashlib.sha256(src.read_bytes()).hexdigest()!=src_hash:
            raise RuntimeError("Original M4A changed unexpectedly: "+str(src))
        tmp.replace(dst)
        records.append({
            "track":name,
            "source":"m4a",
            "output":"ogg/vorbis",
            "samplerate":44100,
            "channels":2,
            "source_bytes":src.stat().st_size,
            "ogg_bytes":dst.stat().st_size,
            "source_sha256":src_hash,
            "ogg_sha256":hashlib.sha256(dst.read_bytes()).hexdigest(),
        })

    a.output.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(a.output,"w",compression=zipfile.ZIP_STORED) as z:
        for name in NAMES:
            z.write(music/(name+".ogg"),"zombieshooter/assets/music/"+name+".ogg")
        z.writestr("zombieshooter/music-ogg-manifest.json",json.dumps(records,indent=2))

    source_total=sum(r["source_bytes"] for r in records)
    ogg_total=sum(r["ogg_bytes"] for r in records)
    print(json.dumps({
        "package":str(a.output.resolve()),
        "tracks":records,
        "source_total_bytes":source_total,
        "ogg_total_bytes":ogg_total,
        "ratio":(ogg_total/source_total if source_total else None),
    },indent=2))


if __name__=="__main__":
    main()
