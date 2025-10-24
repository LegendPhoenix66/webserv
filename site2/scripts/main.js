document.addEventListener('DOMContentLoaded', function () {
	'use strict';
	var chooseBtn = document.getElementById('choose-file');
	var fileInput = document.getElementById('file-input');
	var fileInfo = document.getElementById('file-info');
	var uploadBtn = document.getElementById('do-upload');
	var status = document.getElementById('status');
	var loadFirst = document.getElementById('load-first');
	var open404 = document.getElementById('open-404');

	if (!chooseBtn || !fileInput || !fileInfo || !uploadBtn) {
		// Required elements are missing — nothing to wire.
		return;
	}

	chooseBtn.addEventListener('click', function () { fileInput.click(); });

	fileInput.addEventListener('change', function () {
		if (fileInput.files && fileInput.files.length) {
			fileInfo.textContent = fileInput.files[0].name + ' (' + Math.round(fileInput.files[0].size / 1024) + ' KB)';
		} else {
			fileInfo.textContent = 'No file chosen';
		}
	});

	uploadBtn.addEventListener('click', function () {
		if (!fileInput.files || !fileInput.files.length) {
			if (status) status.textContent = 'Please choose a file first.';
			return;
		}
		if (status) status.textContent = 'Uploading...';
		var fd = new FormData();
		fd.append('file', fileInput.files[0]);
		fetch('/upload', { method: 'POST', body: fd })
			.then(function (resp) {
				if (resp.ok) return resp.text();
				throw new Error('Upload failed: ' + resp.status + ' ' + resp.statusText);
			})
			.then(function (text) {
				if (status) status.textContent = 'Upload successful.';
				console.log('upload response:', text);
			})
			.catch(function (err) {
				if (status) status.textContent = 'Upload error: ' + err.message;
			});
	});

	if (loadFirst) {
		loadFirst.addEventListener('click', function () {
			if (status) status.textContent = 'Loading first.html...';
			fetch('/first.html')
				.then(function (r) {
					if (!r.ok) throw new Error('HTTP ' + r.status);
					return r.text();
				})
				.then(function (html) {
					// extract the main content from the fetched page if possible
					var tmp = document.createElement('div');
					tmp.innerHTML = html;
					var fetchedMain = tmp.querySelector('main');
					var mainContent = document.getElementById('main-content');
					if (fetchedMain && mainContent) {
						mainContent.innerHTML = fetchedMain.innerHTML;
					} else if (mainContent) {
						mainContent.innerHTML = html;
					}
					if (status) status.textContent = 'Loaded first.html';
					document.title = 'site2 — first.html';
				})
				.catch(function (err) {
					if (status) status.textContent = 'Error loading first.html: ' + err.message;
				});
		});
	}

	if (open404) {
		open404.addEventListener('click', function () { window.location.href = '/no-such-page'; });
	}
});
