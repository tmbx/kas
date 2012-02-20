# KWMO modes
MODE_WS = "MODE_WS"
MODE_PUBWS = "MODE_PUBWS"

# This function generates a random string, suitable for a username or password.
def gen_random(nb):
    generator = random.SystemRandom()
    s = ""

    for i in range(nb):
        s += generator.choice(['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
                               'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
                               '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'])
    return s

