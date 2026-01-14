#!/usr/bin/env python3

import argparse
import socket
import threading
import sys
import time


class ConnectionStats:
    """Thread-safe statistics tracking"""
    def __init__(self):
        self.lock = threading.Lock()
        self.successful = 0
        self.failed = 0
        self.connections = []
    
    def add_success(self, sock):
        with self.lock:
            self.successful += 1
            self.connections.append(sock)
    
    def add_failure(self):
        with self.lock:
            self.failed += 1
    
    def get_stats(self):
        with self.lock:
            return {
                'successful': self.successful,
                'failed': self.failed,
                'total_attempted': self.successful + self.failed,
                'open_connections': len(self.connections)
            }


def create_connection(host, port, mode, stats):
    """Create a single connection and leave it abandoned"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)  # 10 second timeout for connection
        
        # Connect to the host
        sock.connect((host, port))
        
        # For telnet/ssh, we just leave the connection open
        # Don't close it - this simulates a bad/abandoned connection
        stats.add_success(sock)
        return sock
    except Exception:
        stats.add_failure()
        return None


def worker_thread(thread_id, host, port, mode, count, stats):
    """Worker thread that creates 'count' connections"""
    thread_connections = []
    
    for i in range(count):
        sock = create_connection(host, port, mode, stats)
        if sock:
            thread_connections.append(sock)
        
        # Print progress every 10 connections
        if (i + 1) % 10 == 0:
            current_stats = stats.get_stats()
            print(f"Thread {thread_id}: Created {i + 1}/{count} connections "
                  f"(Total: {current_stats['successful']} successful, "
                  f"{current_stats['failed']} failed)")
    
    # Keep connections alive by storing them
    # The connections will remain open until the program exits
    return thread_connections


def main():
    parser = argparse.ArgumentParser(
        description='Test blocklist by creating abandoned connections',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --mode telnet --host localhost --port 23 --count 100 --threads 5
  %(prog)s -m ssh -H 192.168.1.1 -p 22 -c 50 -t 10
        """
    )
    
    parser.add_argument(
        '--mode', '-m',
        required=True,
        choices=['telnet', 'ssh'],
        help='Connection mode: telnet or ssh'
    )
    
    parser.add_argument(
        '--host', '-H',
        required=True,
        help='Target hostname or IP address'
    )
    
    parser.add_argument(
        '--port', '-p',
        type=int,
        required=True,
        help='Target port number'
    )
    
    parser.add_argument(
        '--count', '-c',
        type=int,
        required=True,
        help='Number of connections per thread'
    )
    
    parser.add_argument(
        '--threads', '-t',
        type=int,
        required=True,
        help='Number of concurrent threads'
    )
    
    args = parser.parse_args()
    
    # Validate arguments
    if args.count <= 0:
        print("Error: count must be greater than 0", file=sys.stderr)
        sys.exit(1)
    
    if args.threads <= 0:
        print("Error: threads must be greater than 0", file=sys.stderr)
        sys.exit(1)
    
    if args.port < 1 or args.port > 65535:
        print("Error: port must be between 1 and 65535", file=sys.stderr)
        sys.exit(1)
    
    total_connections = args.count * args.threads
    
    print("Starting connection test:")
    print(f"  Mode: {args.mode}")
    print(f"  Host: {args.host}")
    print(f"  Port: {args.port}")
    print(f"  Connections per thread: {args.count}")
    print(f"  Threads: {args.threads}")
    print(f"  Total connections: {total_connections}")
    print()
    
    stats = ConnectionStats()
    threads = []
    start_time = time.time()
    
    # Create and start worker threads
    for i in range(args.threads):
        thread = threading.Thread(
            target=worker_thread,
            args=(i + 1, args.host, args.port, args.mode, args.count, stats)
        )
        thread.daemon = True
        threads.append(thread)
        thread.start()
    
    # Wait for all threads to complete
    for thread in threads:
        thread.join()
    
    elapsed_time = time.time() - start_time
    
    # Print final statistics
    final_stats = stats.get_stats()
    print()
    print("=" * 60)
    print("Connection Test Summary")
    print("=" * 60)
    print(f"Total connections attempted: {final_stats['total_attempted']}")
    print(f"Successful connections: {final_stats['successful']}")
    print(f"Failed connections: {final_stats['failed']}")
    print(f"Connections left open: {final_stats['open_connections']}")
    print(f"Time elapsed: {elapsed_time:.2f} seconds")
    print()
    print(f"All {final_stats['open_connections']} successful connections are")
    print("left open (abandoned) to simulate bad connections.")
    print("Press Ctrl+C to exit and close all connections.")
    
    # Keep the program running to maintain connections
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nExiting... Closing all connections.")
        # Close all connections
        for sock in stats.connections:
            try:
                sock.close()
            except Exception:
                pass
        sys.exit(0)


if __name__ == '__main__':
    main()
