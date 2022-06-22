import mido
import sys

def parse_midi(path_to_midi):
    mid = mido.MidiFile(path_to_midi)
    melody = []
    tempo = 0
    for msg in mid:
        if not msg.is_meta:
            break
        elif msg.type == "set_tempo":
            tempo = mido.tempo2bpm(msg.tempo)
    
    for msg in mid:
        # print(msg)
        if msg.type == 'note_on' or msg.type == 'note_off':
            # print(msg)
            melody.append([(msg.note - 60) % 12, msg.velocity, int(msg.time*1000)])

    return melody

notes = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

melody = parse_midi("midi_pirate2.mid")
file_path = 'output.txt'
sys.stdout = open(file_path, "w")

print("/show_info")
print("/prepare")
print("/turn_compressor;1")

for element in melody:
    # print(element)
    if element[1] == 0:
        print("/play_note;", notes[element[0]], ";", element[2], sep='')
    else:
        print("/delay;", element[2], sep='')

print("/turn_compressor;0")
print("/turn_valve;0")
print("/prepare")