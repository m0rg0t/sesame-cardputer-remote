(() => {
  const copyButton = document.querySelector("[data-copy-checksum]");
  const checksumNode = document.querySelector("[data-checksum]");
  if (!copyButton || !checksumNode) return;

  copyButton.addEventListener("click", async () => {
    const checksum = checksumNode.textContent.trim();
    try {
      await navigator.clipboard.writeText(checksum);
      copyButton.textContent = "Copied";
    } catch {
      copyButton.textContent = "Select hash";
      window.getSelection()?.selectAllChildren(checksumNode);
    }
    window.setTimeout(() => {
      copyButton.textContent = "Copy";
    }, 1600);
  });
})();
