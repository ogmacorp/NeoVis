# ----------------------------------------------------------------------------
#  NeoVis
#  Copyright(c) 2017-2019 Ogma Intelligent Systems Corp. All rights reserved.
#
#  This copy of NeoVis is licensed to you under the terms described
#  in the NEOVIS_LICENSE.md file included in this distribution.
# ----------------------------------------------------------------------------

import socket
import select
import struct
import sys
import threading
import pyogmaneo

class VisAdapter:
    def __init__(self, port=54000):
        self.stop = False

        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        self.listener.bind(("localhost", port))

        self.listener.listen(1)

        self.listen_thread = threading.Thread(target=self.listen, daemon=True)
        self.listen_thread.start()

        self.clients = []

    def listen(self):
        while not self.stop:
            conn, addr = self.listener.accept()

            self.clients.append(( conn, addr ))

            print("Connected!")

    def close(self):
        self.stop = True

        self.listen_thread.join()

        for client in self.clients:
            conn = client[0]

            conn.shutdown(2)
            conn.close()

        self.listener.shutdown(2)
        self.listener.close()

    def update(self, h: pyogmaneo.PyHierarchy):
        new_clients = []
        
        for client in self.clients:
            conn = client[0]

            ready = True

            try:
                ready_to_read, ready_to_write, in_error = select.select([ conn, ], [ conn, ], [], 5)
            except select.error:
                conn.close()

                ready = False

                print("A client disconnected.")

            if ready:    
                if len(ready_to_read) > 0:
                    pass

                if len(ready_to_write) > 0:
                    # Send data
                    num_layers = h.getNumLayers()

                    b = bytearray()

                    b += struct.pack("H", int(num_layers))

                    for l in range(num_layers):
                        bsdr = bytearray()

                        size = h.getHiddenSize(l)

                        width = size.x
                        height = size.y
                        column_size = size.z

                        b += struct.pack("HHH", int(width), int(height), int(column_size))

                        sdr = list(h.getHiddenCs(l))

                        for i in range(width * height):
                            bsdr += struct.pack("H", sdr[i])

                        b += bsdr

                    try:
                        conn.send(b)
                    except Exception:
                        conn.close()

                        print("A client disconnected.")

                        ready = False

            if ready:
                new_clients.append(client)

        self.clients = new_clients
