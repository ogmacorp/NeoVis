# ----------------------------------------------------------------------------
#  NeoVis
#  Copyright(c) 2017-2024 Ogma Intelligent Systems Corp. All rights reserved.
#
#  This copy of NeoVis is licensed to you under the terms described
#  in the NEOVIS_LICENSE.md file included in this distribution.
# ----------------------------------------------------------------------------

import socket
import select
import struct
import sys
import threading
import pyaogmaneo as neo

class VisAdapter:
    def __init__(self, port=54000):
        self.stop = False

        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        self.listener.bind(("0.0.0.0", port))

        self.listener.listen(1)

        self.listen_thread = threading.Thread(target=self._listen, daemon=True)
        self.listen_thread.start()

        self.clients = []

        self.caret = None

    def _listen(self):
        while not self.stop:
            conn, addr = self.listener.accept()

            self.clients.append((conn, addr))

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

    def update(self, h: neo.Hierarchy, encs: [ neo.ImageEncoder ]):
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
                    num_layers = h.get_num_layers()
                    num_encs = len(encs)

                    b = bytearray()

                    b += struct.pack("H", int(num_layers + num_encs))
                    b += struct.pack("H", int(num_encs))

                    # Add encoders
                    for l in range(num_encs):
                        blayer = bytearray()

                        size = encs[l].get_hidden_size()

                        width = size[0]
                        height = size[1]
                        column_size = size[2]

                        blayer += struct.pack("HHH", int(width), int(height), int(column_size))

                        sdr = encs[l].get_hidden_cis()

                        for i in range(width * height):
                            blayer += struct.pack("h", sdr[i])

                        b += blayer

                    for l in range(num_layers):
                        blayer = bytearray()

                        size = h.get_hidden_size(l)

                        width = size[0]
                        height = size[1]
                        column_size = size[2]

                        blayer += struct.pack("HHH", int(width), int(height), int(column_size))

                        sdr = list(h.get_hidden_cis(l))

                        for i in range(width * height):
                            blayer += struct.pack("h", sdr[i])

                        b += blayer

                    assert self.caret is None or (self.caret[0] >= 0 and self.caret[0] < h.get_num_layers() + num_encs)
                    
                    num_fields = 0
                    layer_index = 0

                    if self.caret is not None:
                        layer_index = int(self.caret[0])
                        pos = (self.caret[1], self.caret[2], self.caret[3])
                    
                        if layer_index < num_encs:
                            enc_index = layer_index
                            enc = encs[enc_index]

                            in_bounds = pos[0] >= 0 and pos[1] >= 0 and pos[2] >= 0 and pos[0] < enc.get_hidden_size()[0] and pos[1] < enc.get_hidden_size()[1] and pos[2] < enc.get_hidden_size()[2]

                            num_fields = 0 if not in_bounds else enc.get_num_visible_layers()
                        else:
                            in_bounds = pos[0] >= 0 and pos[1] >= 0 and pos[2] >= 0 and pos[0] < h.get_hidden_size(layer_index - num_encs)[0] and pos[1] < h.get_hidden_size(layer_index - num_encs)[1] and pos[2] < h.get_hidden_size(layer_index - num_encs)[2]

                            num_fields = 0 if not in_bounds else h.get_num_encoder_visible_layers(layer_index - num_encs)
                    
                    b += struct.pack("H", num_fields)

                    if layer_index < num_encs:
                        enc_index = layer_index
                        enc = encs[enc_index]

                        for f in range(num_fields):
                            bfield = bytearray()
                            
                            name = "field" + str(f)

                            bname = name.encode()

                            while len(bname) < 64:
                                bname += struct.pack("c", b"\0")

                            bfield += bname

                            field, field_size = encs[enc_index].get_receptive_field(f, pos)

                            bfield += struct.pack("iii", field_size[0], field_size[1], field_size[2])

                            for i in range(field_size[0] * field_size[1] * field_size[2]):
                                bfield += struct.pack("B", field[i])

                            b += bfield
                    else:
                        for f in range(num_fields):
                            bfield = bytearray()
                            
                            name = "field" + str(f)

                            bname = name.encode()

                            while len(bname) < 64:
                                bname += struct.pack("c", b"\0")

                            bfield += bname

                            field, field_size = h.get_encoder_receptive_field(layer_index - num_encs, f, pos)
     
                            bfield += struct.pack("iii", field_size[0], field_size[1], field_size[2])

                            for i in range(field_size[0] * field_size[1] * field_size[2]):
                                bfield += struct.pack("B", field[i])

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
