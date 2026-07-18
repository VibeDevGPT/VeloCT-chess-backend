import os
import subprocess
import threading
import json
import requests
from flask import Flask

app = Flask(__name__)
LICHESS_TOKEN = os.environ.get("LICHESS_TOKEN")
ENGINE_PATH = "./veloct"

def send_move(game_id, move):
    url = f"https://lichess.org/api/bot/game/{game_id}/move/{move}"
    headers = {"Authorization": f"Bearer {LICHESS_TOKEN}"}
    requests.post(url, headers=headers)

def engine_think(game_id, fen, moves_string):
    try:
        proc = subprocess.Popen(
            [ENGINE_PATH], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1
        )
        proc.stdin.write("uci\n")
        proc.stdin.write("isready\n")
        
        pos_cmd = f"position fen {fen}" if fen else "position startpos"
        if moves_string:
            pos_cmd += f" moves {moves_string}"
        
        proc.stdin.write(f"{pos_cmd}\n")
        proc.stdin.write("go movetime 1500\n")
        proc.stdin.flush()

        while True:
            line = proc.stdout.readline()
            if not line:
                break
            if line.startswith("bestmove"):
                best_move = line.split()[1]
                if best_move != "(none)":
                    send_move(game_id, best_move)
                break
        proc.terminate()
    except Exception as e:
        print(f"Engine error: {e}")

def stream_game(game_id):
    url = f"https://lichess.org/api/bot/game/stream/{game_id}"
    headers = {"Authorization": f"Bearer {LICHESS_TOKEN}"}
    response = requests.get(url, headers=headers, stream=True)
    
    fen = None
    for line in response.iter_lines():
        if line:
            event = json.loads(line.decode('utf-8'))
            if event.get("type") == "gameFull":
                fen = event.get("initialFen")
                state = event.get("state")
            else:
                state = event
            
            moves = state.get("moves", "")
            moves_list = moves.split() if moves else []
            
            if len(moves_list) % 2 == 0:
                threading.Thread(target=engine_think, args=(game_id, fen, moves)).start()

def listen_events():
    url = "https://lichess.org/api/stream/event"
    headers = {"Authorization": f"Bearer {LICHESS_TOKEN}"}
    print("Connecting to Lichess Bot Stream API...")
    try:
        response = requests.get(url, headers=headers, stream=True)
        for line in response.iter_lines():
            if line:
                event = json.loads(line.decode('utf-8'))
                if event.get("type") == "challenge":
                    challenge_id = event["challenge"]["id"]
                    requests.post(f"https://lichess.org/api/challenge/{challenge_id}/accept", headers=headers)
                elif event.get("type") == "gameStart":
                    game_id = event["game"]["id"]
                    threading.Thread(target=stream_game, args=(game_id,)).start()
    except Exception as e:
        print(f"Stream interrupted: {e}")

@app.route('/')
def home():
    return "VeloCT Bot is running."

if __name__ == "__main__":
    if LICHESS_TOKEN:
        threading.Thread(target=listen_events, daemon=True).start()
    else:
        print("Missing LICHESS_TOKEN.")
    
    # Render requires binding to a port
    port = int(os.environ.get("PORT", 10000))
    app.run(host="0.0.0.0", port=port)
