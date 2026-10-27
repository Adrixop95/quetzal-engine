"""Two asset-free, headless engine peers using real IPv4 UDP sockets."""

import os
import selectors
import subprocess
import sys


def run(binary):
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
    peers = []
    try:
        for side in (0, 1):
            peers.append(subprocess.Popen(
                [binary, str(side)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE, text=True, env=env,
            ))
        ports = []
        for peer in peers:
            with selectors.DefaultSelector() as selector:
                selector.register(peer.stdout, selectors.EVENT_READ)
                if not selector.select(timeout=5):
                    raise RuntimeError("Peer did not announce a UDP port")
            line = peer.stdout.readline().split()
            if len(line) != 2 or line[0] != "PORT":
                raise RuntimeError(f"Invalid peer startup: {line}")
            ports.append(int(line[1]))
        if ports[0] == ports[1]:
            raise RuntimeError("Peers must bind separate ports")
        for side, peer in enumerate(peers):
            peer.stdin.write(f"{ports[side ^ 1]}\n")
            peer.stdin.flush()
        results = []
        for peer in peers:
            out, err = peer.communicate(timeout=16)
            if peer.returncode:
                raise RuntimeError(f"Peer failed ({peer.returncode}): {out} {err}")
            result = out.strip().split()
            if len(result) != 4 or result[:2] != ["RESULT", "24"]:
                raise RuntimeError(f"Missing final state: {out} {err}")
            results.append(result)
        if results[0] != results[1]:
            raise RuntimeError(f"Peer state or command count differs: {results}")
        print("Two-process UDP lockstep passed:", " ".join(results[0]))
    finally:
        for peer in peers:
            if peer.poll() is None:
                peer.kill()
            out, err = peer.communicate()
            if peer.returncode and err:
                print(err, file=sys.stderr)


if __name__ == "__main__":
    run(sys.argv[1])
