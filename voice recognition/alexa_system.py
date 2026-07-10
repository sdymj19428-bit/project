import numpy as np
import pyaudio
import requests
import time
import re
import signal
import sys
import os
import subprocess

# 가상환경 경로 우선 순위 설정
sys.path.insert(0, "/home/ubuntu/oww_env/lib/python3.10/site-packages")

from openwakeword.model import Model
from faster_whisper import WhisperModel

# --- 설정 (Configuration) ---
VOICE_DIR = "/home/ubuntu/voice_test/voices"
MODEL_PATH = "/home/ubuntu/oww_env/lib/python3.10/site-packages/openwakeword/resources/models/alexa_v0.1.tflite"
WAKEWORD_NAME = "alexa_v0.1" 
WHISPER_MODEL_SIZE = "base"
RASA_API_URL = "http://localhost:5005/model/parse"
RASA_WEBHOOK_URL = "http://localhost:5005/webhooks/rest/webhook"

#모델이 동작 될 경계점
WAKE_THRESHOLD = 0.5    
RASA_THRESHOLD = 0.6    

# 위스퍼 성능 향상을 위한 프롬프트
WHISPER_PROMPT = "철수, 영희, 길동, 이리로 와, 멈춰, 집으로 가, 구역, 이동해, 로봇 제어 명령."

# 로봇 이름이 꼭 필요한 인텐트 목록
NAME_REQUIRED_INTENTS = ["come_here", "stop", "go_home"]

# --- 음성 파일 재생 함수 ---
def play_voice_and_wait(filename, stream_obj):
    file_path = os.path.join(VOICE_DIR, filename)
    if os.path.exists(file_path):
        if stream_obj.is_active():
            stream_obj.stop_stream()
        subprocess.run(["mpg123", "-q", file_path])
        time.sleep(0.1) 
        stream_obj.start_stream()
    else:
        print(f"[경고] 파일 없음: {filename}", flush=True)

# --- Rasa 분석 함수 ---
def get_rasa_all_info(text):
    try:
        parse_res = requests.post(RASA_API_URL, json={"text": text}, timeout=3).json()
        intent = parse_res.get("intent", {}).get("name", "nlu_fallback")
        confidence = parse_res.get("intent", {}).get("confidence", 0)
        
        robot_name = "NULL"
        for ent in parse_res.get("entities", []):
            if ent.get("entity") == "robot_name":
                robot_name = ent.get("value")
                break
        
        session_id = f"user_{time.time()}"
        reply_res = requests.post(RASA_WEBHOOK_URL, json={"sender": session_id, "message": text}, timeout=3).json()
        response_text = reply_res[0].get('text', "NULL") if reply_res else "NULL"
            
        return intent, confidence, robot_name, response_text
    except:
        return "error", 0, "NULL", "서버 연결 오류"

# --- 모델 초기화 ---
print("모델 로딩 중...", flush=True)
oww_model = Model(wakeword_models=[MODEL_PATH], inference_framework="tflite")

# Whisper 엔진 최적화 로딩
stt_model = WhisperModel(
    WHISPER_MODEL_SIZE, 
    device="cpu", 
    compute_type="int8", 
    cpu_threads=1,          # CPU 부하 분산 방지
    local_files_only=False  # 필요시 모델 업데이트 확인
)

# --- 오디오 설정 ---
FORMAT, CHANNELS, RATE, CHUNK = pyaudio.paInt16, 1, 16000, 1280
audio = pyaudio.PyAudio()

def timeout_handler(signum, frame):
    raise TimeoutError("STT Timeout")

signal.signal(signal.SIGALRM, timeout_handler)

current_state = "WAKE"
stream = audio.open(format=FORMAT, channels=CHANNELS, rate=RATE, input=True, frames_per_buffer=CHUNK)

is_waiting_printed = False 

try:
    while True:
        if current_state == "WAKE":
            if not is_waiting_printed:
                print("대기중", flush=True)
                is_waiting_printed = True

            data = stream.read(CHUNK, exception_on_overflow=False)
            audio_frame = np.frombuffer(data, dtype=np.int16)
            prediction = oww_model.predict(audio_frame)
            
            if prediction[WAKEWORD_NAME] > WAKE_THRESHOLD:
                print("듣는중", flush=True)
                play_voice_and_wait("listening.mp3", stream) 
                current_state = "LISTEN"
                is_waiting_printed = False

        elif current_state == "LISTEN":
            frames = []
            for _ in range(0, int(RATE / CHUNK * 3)): 
                frames.append(stream.read(CHUNK, exception_on_overflow=False))
            
            stream.stop_stream()
            audio_data = np.frombuffer(b''.join(frames), dtype=np.int16).astype(np.float32) / 32768.0
            
            try:
                signal.alarm(8) # 연산 시간을 8초로 제한
                
                #Whisper 추론 파라미터 최적화
                segments, info = stt_model.transcribe(
                    audio_data, 
                    language="ko", 
                    beam_size=1, 
                    initial_prompt=WHISPER_PROMPT,  # 정확도 대폭 향상
                    vad_filter=True,                # 정밀 VAD 활성화
                    vad_parameters=dict(min_silence_duration_ms=500), # 0.5초 무음 시 컷
                    no_speech_threshold=0.6,        # 말소리 아닌 소리 무시
                    condition_on_previous_text=False
                )
                
                text = "".join([s.text for s in segments]).strip()
                signal.alarm(0)

                # 말소리를 재외하고, 불필요한 데이터를 제거
                clean_text = re.sub(r'[^가-힣a-zA-Z0-9\s]', '', text).strip()
                
                if len(clean_text) < 2:
                    print("잡음", flush=True)
                    play_voice_and_wait("retry.mp3", stream)
                else:
                    intent, confidence, robot_name, response_text = get_rasa_all_info(text)
                    
                    #정확도가 떨어지거나 의도가 불분명하면 실패
                    if intent != "nlu_fallback" and confidence >= RASA_THRESHOLD:
                        #지정한 format으로 필요한 데이터를 print하여 qt에서 사용할 수 있도록 함
                        #semi json 방식
                        print(f"intent: {intent}, confidence: {confidence:.2f}, robot_name: {robot_name}, text: {text}, response: {response_text}", flush=True)
                        
                        # 의도 정규화 (go_A_auto -> go_A)
                        if intent.startswith("go_"):
                            parts = intent.split('_')
                            if len(parts) >= 2:
                                intent = f"{parts[0]}_{parts[1]}"
                        
                        if intent in NAME_REQUIRED_INTENTS and robot_name != "NULL":
                            target_file = f"{robot_name}_{intent}.mp3"
                        else:
                            target_file = f"{intent}.mp3"
                            
                        play_voice_and_wait(target_file, stream)
                    else:
                        print("인식 실패", flush=True)
                        play_voice_and_wait("retry.mp3", stream) 

            except TimeoutError:
                print("타임아웃", flush=True)
                play_voice_and_wait("timeout_error.mp3", stream)
                
            except Exception as e:
                print(f"오류 발생: {e}", flush=True)
                play_voice_and_wait("whisper_error.mp3", stream)
            
            current_state = "WAKE"
            oww_model.reset()
            is_waiting_printed = False 
            stream.start_stream() 
            time.sleep(0.2) 

except KeyboardInterrupt:
    print("\n시스템 종료", flush=True)
    stream.stop_stream()
    stream.close()
    audio.terminate()
