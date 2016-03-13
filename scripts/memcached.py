#!python


class Slab:
  def __init__(self, chk, sid):
    self.chunk_size = chk
    self.id = sid
  def __eq__(self, other):
    return self.sid == other.sid
  objs_cached=0
  bytes_cached=0
  hits=0
  def frag ():
    return 1 - (bytes_cached/(objs_cached * chunk_size))

class SlabClass:
  def __init__(self, chk):
    self.chunk_size = chk
    self.slabs = [Slab(chk, 0)]
  hits=0
  slabs=1
  def add_slab ():
    self.slabs.append(Slab(self.chunk_size, ++slabs))
  def kill_slab (sid):
    self.slabs.remove(sid)


  

