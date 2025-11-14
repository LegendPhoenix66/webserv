This folder is included for local testing of uploads.

Note: the server configuration in `conf_files/test.conf` uses an absolute path `upload_store /uploads`.
If your server maps uploads to an absolute path, either adjust the config to point to `./site2/uploads` for local testing
or create a matching `/uploads` path on the system where the server runs.
<!doctype html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title>Site2 — Home</title>
    <link rel="stylesheet" href="/styles/shared.css">
    <link rel="stylesheet" href="/styles/style.css">
</head>
<body>
    <header>
        <h1>Welcome to site2 (port 8080)</h1>
        <nav>
            <a href="/">Home</a> |
            <a href="/images/">Images</a> |
            <a href="/upload">Upload</a> |
            <a href="/cgi-bin/">CGI</a>
        </nav>
    </header>

    <main>
        <section>
            <h2>Main index (index.html)</h2>
            <p>This is the default index served from <code>./site2</code>. According to the server config the directory also accepts <code>index.htm</code> and the location root prefers <code>first.html</code>.</p>
            <p>Try: <a href="/first.html">the location-specific index (first.html)</a></p>
        </section>

        <section>
            <h3>Upload form (POST to /upload)</h3>
            <form action="/upload" method="post" enctype="multipart/form-data">
                <label for="file">Choose file</label>
                <input id="file" name="file" type="file">
                <button type="submit">Upload</button>
            </form>
            <p>(The server's config sets <code>upload_store /uploads</code> — for local testing a <code>site2/uploads</code> folder is included.)</p>
        </section>

        <section>
            <h3>CGI demo link</h3>
            <p>The config maps <code>/cgi-bin</code> to a CGI handler (Python). A link to <code>/cgi-bin/hello.py</code> is provided for integration testing.</p>
            <p><a href="/cgi-bin/hello.py">Run CGI hello (if CGI is enabled)</a></p>
        </section>
    </main>

    <footer>
        <small>site2 — test content for webserv configuration</small>
    </footer>
</body>
</html>
