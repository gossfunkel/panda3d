from direct.showbase.ShowBase import ShowBase
from panda3d.core import load_prc_file_data
load_prc_file_data("", "audio-library-name p3mini_audio")

app = ShowBase()
app.run()