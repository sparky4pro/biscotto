document.querySelectorAll("pre > code").forEach(code_block => {
	const pre = code_block.parentElement;

	const button = document.createElement("button");
	button.className = "copy-button";
	button.textContent = "Copy";

	button.addEventListener("click", async () => {
		try {
			await navigator.clipboard.writeText(code_block.innerText);
			button.textContent = "Copied";
			button.classList.add("copied");

			setTimeout(() => {
				button.textContent = "Copy";
				button.classList.remove("copied");
			}, 1500);
		} catch (err) {
			button.textContent = "Error";
		}
	});

	pre.appendChild(button);
});