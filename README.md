# mqtt_P10_matrix
Modified PixelTime for use with mqtt

I have used this [driver](https://github.com/2dom/P10_matrix) and modified the code to use mqtt to be able to print out messages from subscribed topic instead of weather. Improvements are coming, this is a first draft (and the first thing I've ever "written" for arduino).

When downloaded you need to supply your own info for ssid and password for wifi (two separate locations for now), and your mqtt broker info and login if applicable. I have commented with "//change" on those spots, so do a search in the file.
You also need to change the "client.subscribe" and "client.publish" topics to those you like to have.
