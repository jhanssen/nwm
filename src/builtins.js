nwm.on("layout", function(client) {
    var rect = nwm.screens[client.screen];
    console.log("got client for layout", client, rect);
    client.move(0, 0);
    client.resize(rect.width, rect.height);
});
