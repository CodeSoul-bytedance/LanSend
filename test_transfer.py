#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import json
import socket
import uuid
import platform
import time
import argparse
import ssl
import os
import threading
import logging
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse

# --- Global Configuration & State ---
SERVER_PORT = 56789
BROADCAST_PORT_SERVER = 56789 # Port on which this server listens for discovery broadcasts (not used by this script directly for listening)
BROADCAST_PORT_CLIENT = 56789 # Port to which LanSend clients listen for discovery
BROADCAST_INTERVAL_SECONDS = 3
EXPECTED_PIN_CODE = "123456"  # Default PIN, configurable via args
OUTPUT_DIRECTORY = "received_files"
CERT_FILE_PATH = "cert.pem"
KEY_FILE_PATH = "key.pem"

MY_DEVICE_INFO = {}
CONNECTED_CLIENT_DEVICE_INFO = None
CURRENT_TRANSFER_SESSION_ID = None
# ACTIVE_TRANSFERS will store: {file_id: {"name": ..., "size": ..., "path": ..., "temp_file_path": ..., "received_bytes": ...}}
ACTIVE_TRANSFERS = {}

# Setup logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

def get_local_ip():
    """获取本机IP地址"""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(('10.255.255.255', 1))
        ip = s.getsockname()[0]
    except Exception:
        ip = '127.0.0.1'
    finally:
        s.close()
    return ip

def initialize_device_info(port):
    global MY_DEVICE_INFO
    MY_DEVICE_INFO = {
        "device_id": f"{int(time.time() * 1000)}_{uuid.uuid4().hex[:8]}",
        "alias": "Python Test Device",
        "hostname": socket.gethostname(),
        "operating_system": platform.system(),
        "ip_address": get_local_ip(),
        "port": port,
        "device_type": "desktop", # As per LocalSend spec (e.g., desktop, laptop, tablet, phone)
        "device_model": "SimulatedPC/Python"
    }
    logging.info(f"Device info initialized: {json.dumps(MY_DEVICE_INFO, indent=2, ensure_ascii=False)}")

def broadcast_device_info_periodically(stop_event):
    """在单独的线程中定期广播设备信息"""
    broadcast_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    broadcast_socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    # broadcast_socket.settimeout(1.0) # Avoid blocking indefinitely if needed

    broadcast_address = ('255.255.255.255', BROADCAST_PORT_CLIENT)
    logging.info(f"Starting UDP broadcast on port {BROADCAST_PORT_CLIENT} every {BROADCAST_INTERVAL_SECONDS}s for device: {MY_DEVICE_INFO.get('alias')}")

    while not stop_event.is_set():
        try:
            data_to_send = json.dumps(MY_DEVICE_INFO).encode('utf-8')
            broadcast_socket.sendto(data_to_send, broadcast_address)
            logging.debug(f"Broadcasted device info: {MY_DEVICE_INFO.get('alias')}")
        except Exception as e:
            logging.error(f"Error during UDP broadcast: {e}")
        time.sleep(BROADCAST_INTERVAL_SECONDS)
    
    broadcast_socket.close()
    logging.info("UDP broadcast thread stopped.")

class TransferRequestHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1" # Add this line

    def _send_json_response(self, status_code, data, extra_headers=None):
        self.send_response(status_code)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Connection', 'close') # Add this line
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'POST, GET, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type, X-Session-ID, X-File-ID, X-Chunk-Offset') # Add other headers LanSend might send
        if extra_headers:
            for key, value in extra_headers.items():
                self.send_header(key, value)
        self.end_headers()
        self.wfile.write(json.dumps(data, ensure_ascii=False).encode('utf-8'))
        self.wfile.flush() # Add this line

        try:
            if hasattr(self.request, 'unwrap') and callable(getattr(self.request, 'unwrap')):
                logging.debug("Attempting SSL unwrap for graceful shutdown.")
                self.request.unwrap()
            # else: # No need for an else if it's not an SSL socket, _send_json_response is only used by SSL server part
            #    logging.debug("No unwrap method found on self.request, or not an SSL socket as expected.")
        except ssl.SSLError as e_ssl:
            # SSLError can happen if client already closed connection or other SSL issues during shutdown
            if "UNEXPECTED_EOF_WHILE_READING" in str(e_ssl):
                logging.debug(f"SSL unwrap non-critical EOF (client likely closed connection): {e_ssl}")
            else:
                logging.warning(f"SSLError during unwrap: {e_ssl}")
        except Exception as e_un:
            logging.error(f"Unexpected error during unwrap: {e_un}")

    def do_OPTIONS(self):
        # Pre-flight request for CORS
        self._send_json_response(204, "") 

    def do_POST(self):
        global CONNECTED_CLIENT_DEVICE_INFO, CURRENT_TRANSFER_SESSION_ID, ACTIVE_TRANSFERS
        parsed_path = urlparse(self.path)
        
        content_length = int(self.headers.get('Content-Length', 0))
        post_data_raw = self.rfile.read(content_length)

        endpoint_path = parsed_path.path
        logging.info(f"POST request to: {endpoint_path} from {self.client_address}")

        if endpoint_path == '/connect': 
            try:
                data = json.loads(post_data_raw.decode('utf-8'))
                pin_code = data.get('pin_code')
                client_info = data.get('device_info')

                if not pin_code or not client_info:
                    logging.warning("Connect request missing pin_code or device_info.")
                    self._send_json_response(400, {"message": "Missing pin_code or device_info"})
                    return

                if pin_code == EXPECTED_PIN_CODE:
                    CONNECTED_CLIENT_DEVICE_INFO = client_info
                    logging.info(f"Connection accepted from: {client_info.get('alias', 'Unknown Device')} ({client_info.get('device_id')})")
                    # Respond with a success message including the boolean 'success' key
                    success_response = {
                        "success": True,
                        "message": "Connection accepted by Python simulator.",
                        "device_info": MY_DEVICE_INFO
                    }
                    self._send_json_response(200, success_response)
                else:
                    logging.warning(f"Invalid PIN received: {pin_code}. Expected: {EXPECTED_PIN_CODE}")
                    self._send_json_response(401, {"message": "Invalid PIN code"})
            except json.JSONDecodeError:
                logging.error("Invalid JSON in /connect request.")
                self._send_json_response(400, {"message": "Invalid JSON format"})
            except Exception as e:
                logging.error(f"Error in /connect: {e}", exc_info=True)
                self._send_json_response(500, {"message": "Internal server error"})

        elif endpoint_path == '/request-send':
            if not CONNECTED_CLIENT_DEVICE_INFO:
                logging.warning("Send_request attempt without prior /connect.")
                self._send_json_response(403, {"message": "No active connection. Please /connect first."}) 
                return
            try:
                data = json.loads(post_data_raw.decode('utf-8'))
                session_id_req = data.get('session_id') # LanSend sends this
                files_map = data.get('files') # map of file_id to FileMetadataRequest

                if not session_id_req or not isinstance(files_map, dict):
                    logging.warning("Send_request missing session_id or files, or invalid files format.")
                    self._send_json_response(400, {"message": "Missing session_id or files, or invalid files format"})
                    return
                
                CURRENT_TRANSFER_SESSION_ID = session_id_req # Use the session_id from the request
                accepted_files_status = {} # file_id -> bool
                
                if not os.path.exists(OUTPUT_DIRECTORY):
                    os.makedirs(OUTPUT_DIRECTORY)
                    logging.info(f"Created output directory: {OUTPUT_DIRECTORY}")

                ACTIVE_TRANSFERS.clear() # Clear previous transfers for a new session
                for file_id_str, metadata in files_map.items():
                    # file_id from LanSend is string, ensure consistency
                    original_name = metadata.get('name', f"{file_id_str}_unknown")
                    safe_filename = os.path.basename(original_name) # Basic sanitization
                    file_path = os.path.join(OUTPUT_DIRECTORY, safe_filename)
                    temp_file_path = file_path + ".part"
                    
                    ACTIVE_TRANSFERS[file_id_str] = {
                        "name": original_name,
                        "size": int(metadata.get('size', 0)), # Ensure size is int
                        "type": metadata.get('type', 'generic'),
                        "hash": metadata.get('hash'), 
                        "path": file_path,
                        "temp_file_path": temp_file_path,
                        "received_bytes": 0
                    }
                    accepted_files_status[file_id_str] = True # Accept all files
                    logging.info(f"Accepting file: ID='{file_id_str}', Name='{original_name}', Size={metadata.get('size')}. Saving to {temp_file_path} -> {file_path}")
                
                # SendRequestResponse model
                response_payload = {
                    "session_id": CURRENT_TRANSFER_SESSION_ID,
                    "accepted_files": accepted_files_status
                }
                self._send_json_response(200, response_payload)

            except json.JSONDecodeError:
                logging.error("Invalid JSON in /send_request.")
                self._send_json_response(400, {"message": "Invalid JSON format for send_request"})
            except Exception as e:
                logging.error(f"Error in /send_request: {e}", exc_info=True)
                self._send_json_response(500, {"message": "Internal server error during send_request"})
        
        elif parsed_path.path.startswith('/file_data/'): 
            path_parts = parsed_path.path.strip('/').split('/')
            if len(path_parts) == 4 and path_parts[0:3] == ['api', 'v1', 'file_data']:
                file_id_req = path_parts[3] # This is the file_id from the URL
                session_id_header = self.headers.get('X-Session-ID')

                if session_id_header != CURRENT_TRANSFER_SESSION_ID:
                    logging.warning(f"Session ID mismatch for file_data. URL_FileID: {file_id_req}. Expected Session: {CURRENT_TRANSFER_SESSION_ID}, Got Session: {session_id_header}")
                    self._send_json_response(400, {"message": "Session ID mismatch or invalid session"})
                    return

                if file_id_req in ACTIVE_TRANSFERS:
                    transfer_info = ACTIVE_TRANSFERS[file_id_req]
                    try:
                        with open(transfer_info["temp_file_path"], "ab") as f: # Append binary
                            f.write(post_data_raw)
                        transfer_info["received_bytes"] += len(post_data_raw)
                        
                        is_complete = transfer_info["received_bytes"] >= transfer_info["size"]
                        status_msg = "completed" if is_complete else "receiving"
                        
                        logging.info(f"File data: ID='{file_id_req}', Name='{transfer_info['name']}', Received {len(post_data_raw)} bytes. Total: {transfer_info['received_bytes']}/{transfer_info['size']}. Status: {status_msg}")

                        if is_complete:
                            # Ensure temp file is closed before renaming
                            # This is handled by 'with open' context manager
                            final_path = transfer_info["path"]
                            if os.path.exists(final_path): # Handle pre-existing file (e.g. from a retry)
                                os.remove(final_path)
                            os.rename(transfer_info["temp_file_path"], final_path)
                            logging.info(f"File '{transfer_info['name']}' (ID: {file_id_req}) received completely and saved to '{final_path}'.")
                            # Optionally, verify hash here if one was provided and calculated

                        # Respond with StatusResponse
                        status_payload = {
                            "session_id": CURRENT_TRANSFER_SESSION_ID,
                            "file_id": file_id_req,
                            "status": status_msg,
                            "processed_bytes": transfer_info["received_bytes"], # LanSend expects processed_bytes
                            "message": "Chunk processed successfully" if status_msg == "receiving" else "File transfer completed"
                        }
                        self._send_json_response(200, status_payload)

                    except Exception as e:
                        logging.error(f"Error writing file data for {file_id_req}: {e}", exc_info=True)
                        status_payload = {
                            "session_id": CURRENT_TRANSFER_SESSION_ID,
                            "file_id": file_id_req,
                            "status": "error",
                            "processed_bytes": transfer_info.get("received_bytes", 0),
                            "message": str(e)
                        }
                        self._send_json_response(500, status_payload)
                else:
                    logging.warning(f"Unknown file_id in file_data: {file_id_req}. Active transfers: {list(ACTIVE_TRANSFERS.keys())}")
                    self._send_json_response(404, {"message": f"File ID {file_id_req} not found or not accepted in current session"})
            else:
                logging.warning(f"Malformed file_data path: {parsed_path.path}")
                self._send_json_response(404, {"message": "Malformed file_data endpoint path"})
        else:
            logging.warning(f"Unknown POST endpoint: {endpoint_path}")
            self._send_json_response(404, {"message": "Endpoint not found"})

    def log_message(self, format, *args):
        # Suppress default BaseHTTPServer logging to use our own logger
        # logging.info("%s - - [%s] %s\n" % (self.address_string(), self.log_date_time_string(), format%args))
        return

def main():
    global SERVER_PORT, BROADCAST_INTERVAL_SECONDS, EXPECTED_PIN_CODE, OUTPUT_DIRECTORY, CERT_FILE_PATH, KEY_FILE_PATH, BROADCAST_PORT_CLIENT

    parser = argparse.ArgumentParser(description="Simulate a LanSend discoverable device with file transfer capabilities.")
    parser.add_argument("--port", type=int, default=SERVER_PORT, help=f"Port for the HTTPS server (default: {SERVER_PORT})")
    parser.add_argument("--bcast-port", type=int, default=BROADCAST_PORT_CLIENT, help=f"UDP broadcast port for discovery (LanSend clients listen on this port) (default: {BROADCAST_PORT_CLIENT})")
    parser.add_argument("--bcast-interval", type=int, default=BROADCAST_INTERVAL_SECONDS, help=f"UDP broadcast interval in seconds (default: {BROADCAST_INTERVAL_SECONDS})")
    parser.add_argument("--pin", type=str, default=EXPECTED_PIN_CODE, help=f"PIN code for connection authentication (default: '{EXPECTED_PIN_CODE}')")
    parser.add_argument("--output-dir", type=str, default=OUTPUT_DIRECTORY, help=f"Directory to save received files (default: '{OUTPUT_DIRECTORY}')")
    parser.add_argument("--certfile", type=str, default=CERT_FILE_PATH, help=f"Path to SSL certificate file (default: '{CERT_FILE_PATH}')")
    parser.add_argument("--keyfile", type=str, default=KEY_FILE_PATH, help=f"Path to SSL private key file (default: '{KEY_FILE_PATH}')")
    parser.add_argument("--verbose", "-v", action="store_true", help="Enable debug logging")

    args = parser.parse_args()

    SERVER_PORT = args.port
    BROADCAST_PORT_CLIENT = args.bcast_port
    BROADCAST_INTERVAL_SECONDS = args.bcast_interval
    EXPECTED_PIN_CODE = args.pin
    OUTPUT_DIRECTORY = args.output_dir
    CERT_FILE_PATH = "C:/Users/null/Downloads/certificate.pem"
    KEY_FILE_PATH = "C:/Users/null/Downloads/private_key.pem"
    
    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)
        logging.debug("Verbose logging enabled.")

    if not os.path.exists(CERT_FILE_PATH) or not os.path.exists(KEY_FILE_PATH):
        logging.error(f"SSL certificate ('{CERT_FILE_PATH}') or key ('{KEY_FILE_PATH}') not found.")
        logging.error("Please generate them, e.g., using OpenSSL: openssl req -new -x509 -keyout key.pem -out cert.pem -days 365 -nodes")
        logging.error("And ensure they are in the same directory as the script, or specify paths via --certfile and --keyfile.")
        return

    initialize_device_info(SERVER_PORT) # Initialize MY_DEVICE_INFO with the correct port

    stop_broadcast_event = threading.Event()
    broadcast_thread = threading.Thread(target=broadcast_device_info_periodically, args=(stop_broadcast_event,), name="BroadcastThread")
    broadcast_thread.daemon = True # So it exits when main thread exits
    broadcast_thread.start()

    # Create output directory if it doesn't exist
    if not os.path.exists(OUTPUT_DIRECTORY):
        try:
            os.makedirs(OUTPUT_DIRECTORY)
            logging.info(f"Created output directory: {os.path.abspath(OUTPUT_DIRECTORY)}")
        except OSError as e:
            logging.error(f"Could not create output directory '{OUTPUT_DIRECTORY}': {e}")
            # Optionally, exit if directory creation fails and is critical
            # return 

    httpd = HTTPServer(('0.0.0.0', SERVER_PORT), TransferRequestHandler)
    
    # SSL Context for more modern TLS versions and ciphers
    ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    try:
        ssl_context.load_cert_chain(certfile=CERT_FILE_PATH, keyfile=KEY_FILE_PATH)
        # You might want to set specific TLS versions or ciphers here for better security
        # e.g., ssl_context.minimum_version = ssl.TLSVersion.TLSv1_2
    except FileNotFoundError:
        logging.error(f"Error loading SSL cert/key. Cert: '{CERT_FILE_PATH}', Key: '{KEY_FILE_PATH}'. Make sure paths are correct.")
        stop_broadcast_event.set()
        broadcast_thread.join(timeout=2)
        return
    except ssl.SSLError as e:
        logging.error(f"SSL Error loading cert/key: {e}. Check if files are valid PEM format and match.")
        stop_broadcast_event.set()
        broadcast_thread.join(timeout=2)
        return

    httpd.socket = ssl_context.wrap_socket(httpd.socket, server_side=True)
    
    logging.info(f"HTTPS Test Server started on https://{get_local_ip()}:{SERVER_PORT}")
    logging.info(f"PIN Code for connection: {EXPECTED_PIN_CODE}")
    logging.info(f"Receiving files into: {os.path.abspath(OUTPUT_DIRECTORY)}")
    logging.info("Press Ctrl+C to stop the server.")

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        logging.info("Keyboard interrupt received, shutting down...")
    finally:
        logging.info("Stopping broadcast thread...")
        stop_broadcast_event.set()
        broadcast_thread.join(timeout=BROADCAST_INTERVAL_SECONDS + 1) # Wait for broadcast thread to finish
        
        logging.info("Closing HTTP server...")
        httpd.server_close()
        logging.info("Server shut down gracefully.")

if __name__ == "__main__":
    main()