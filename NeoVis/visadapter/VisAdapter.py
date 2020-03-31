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
from pyogmaneo import Int3

class VisAdapter:
    def __init__(self, port=54000):
        self.stop = False

        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        self.listener.bind(("localhost", port))

        self.listener.listen(1)

        self.listen_thread = threading.Thread(target=self._listen, daemon=True)
        self.listen_thread.start()

        self.clients = []

        self.caret = None

    def _listen(self):
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

    def update(self, cs: pyogmaneo.ComputeSystem, h: pyogmaneo.Hierarchy, encs: [ pyogmaneo.ImageEncoder ]):
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
                while ready and len(ready_to_read) > 0:
                    try:
                        b = bytearray()
                        
                        while len(b) < 16:
                            b += conn.recv(16 - len(b))

                        self.caret = struct.unpack("Hiii", b)
                    except Exception:
                        conn.close()

                        # print("Corrupted carret.")

                        ready = False

                    # Read remainder
                    if ready:
                        try:
                            ready_to_read, ready_to_write, in_error = select.select([ conn, ], [ conn, ], [], 5)
                        except select.error:
                            conn.close()

                            ready = False

                            print("A client disconnected.")

                if ready and len(ready_to_write) > 0:
                    # Send data
                    num_layers = h.getNumLayers()
                    num_encs = len(encs)

                    b = bytearray()

                    b += struct.pack("H", int(num_layers + num_encs))
                    b += struct.pack("H", int(num_encs))

                    # Add encoders
                    for l in range(num_encs):
                        blayer = bytearray()

                        size = encs[l].getHiddenSize()

                        width = size.x
                        height = size.y
                        column_size = size.z

                        blayer += struct.pack("HHH", int(width), int(height), int(column_size))

                        sdr = list(encs[l].getHiddenCs())

                        for i in range(width * height):
                            blayer += struct.pack("H", sdr[i])

                        b += blayer

                    for l in range(num_layers):
                        blayer = bytearray()

                        size = h.getHiddenSize(l)

                        width = size.x
                        height = size.y
                        column_size = size.z

                        blayer += struct.pack("HHH", int(width), int(height), int(column_size))

                        sdr = list(h.getHiddenCs(l))

                        for i in range(width * height):
                            blayer += struct.pack("H", sdr[i])

                        b += blayer

                    assert(self.caret is None or (self.caret[0] >= 0 and self.caret[0] < h.getNumLayers() + num_encs))
                    
                    num_fields = 0
                    layerIndex = 0

                    if self.caret is not None:
                        layerIndex = int(self.caret[0])
                        pos = Int3(*self.caret[1:4])
                    
                        if layerIndex < num_encs:
                            enc_index = layerIndex
                            enc = encs[enc_index]
                            inBounds = pos.x >= 0 and pos.y >= 0 and pos.z >= 0 and pos.x < enc.getHiddenSize().x and pos.y < enc.getHiddenSize().y and pos.z < enc.getHiddenSize().z

                            num_fields = 0 if not inBounds else enc.getNumVisibleLayers()
                        else:
                            inBounds = pos.x >= 0 and pos.y >= 0 and pos.z >= 0 and pos.x < h.getHiddenSize(layerIndex - num_encs).x and pos.y < h.getHiddenSize(layerIndex - num_encs).y and pos.z < h.getHiddenSize(layerIndex - num_encs).z

                            num_fields = 0 if not inBounds else h.getNumSCVisibleLayers(layerIndex - num_encs)
                    
                    b += struct.pack("H", num_fields)

                    if layerIndex < num_encs:
                        enc_index = layerIndex
                        enc = encs[enc_index]

                        for f in range(num_fields):
                            bfield = bytearray()
                            
                            name = "field" + str(f)

                            bname = name.encode()

                            while len(bname) < 64:
                                bname += struct.pack("c", b"\0")

                            bfield += bname

                            fieldSize = pyogmaneo.Int3()

                            field = encs[enc_index].getReceptiveField(cs, f, pos, fieldSize)

                            bfield += struct.pack("iii", fieldSize.x, fieldSize.y, fieldSize.z)

                            for i in range(fieldSize.x * fieldSize.y * fieldSize.z):
                                bfield += struct.pack("f", field[i])

                            b += bfield
                    else:
                        for f in range(num_fields):
                            bfield = bytearray()
                            
                            name = "field" + str(f)

                            bname = name.encode()

                            while len(bname) < 64:
                                bname += struct.pack("c", b"\0")

                            bfield += bname

                            fieldSize = pyogmaneo.Int3()

                            field = h.getSCReceptiveField(cs, layerIndex - num_encs, f, pos, fieldSize)
     
                            bfield += struct.pack("iii", fieldSize.x, fieldSize.y, fieldSize.z)

                            for i in range(fieldSize.x * fieldSize.y * fieldSize.z):
                                bfield += struct.pack("f", field[i])

                            b += bfield

                    try:
                        conn.send(b)
                    except Exception:
                        conn.close()

                        print("A client disconnected.")

                        ready = False

            if ready:
                new_clients.append(client)

        self.clients = new_clients
