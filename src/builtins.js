nwm.on("layout", function(client) {
    var rect = nwm.screens[client.screen];
    console.log("got client for layout", client, rect);
    var clientRect;
    if (!client.movable) {
        client.move(0, 0);
        client.resize(rect.width, rect.height);
    } else {
        var x = (rect.width - client.rect.width) / 2;
        var y = (rect.height - client.rect.height) / 2;
        console.log("movable client", client, x, y);
        client.move(x, y);
    }
});
