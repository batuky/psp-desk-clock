# Software-render the Clock face into a pixel buffer

The core draws the Clock face (seven-segment digits, bitmap text, Month calendar) into a plain 480x272 pixel buffer in pure C, and a thin PSP layer only copies that buffer to the screen. We chose this over libGU or the PSP debug screen so the same code runs on the host and the Clock face can be tested by comparing images; at one redraw per minute the CPU cost is negligible.
