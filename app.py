import os
import threading
import subprocess
from flask import Flask

app = Flask(__name__)

@app.route("/")
def home():
    return "VeloCT Engine is running flawlessly 24/7!"

def run_bot():
    # Automatically give execute permissions to your x86_64 binary inside the releases folder
    try:
        os.chmod("./releases/my_bot_x86_64", 0o755)
        print("Set execute permissions on binary successfully.")
    except Exception as e:
        print(f"Error setting permissions: {e}")

    # Launch the lichess-bot driver module loop
    subprocess.run(["python3", "-m", "lichess_bot.main"])

if __name__ == "__main__":
    # Run the Lichess bridge in a background execution thread
    threading.Thread(target=run_bot, daemon=True).start()
    
    # Read dynamic port provided by Koyeb routing mesh
    port = int(os.environ.get("PORT", 8000))
    app.run(host="0.0.0.0", port=port)
    
