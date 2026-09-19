(() => {
  const poses = [
    "REST", "STAND", "WAVE", "DANCE", "CUTE", "BOW", "SHAKE", "SHRUG",
    "POINT", "SWIM", "PUSHUP", "CRAB", "WORM", "FREAKY", "DEAD",
  ];
  const directionForKey = {
    w: "forward",
    ArrowUp: "forward",
    a: "left",
    ArrowLeft: "left",
    s: "backward",
    ArrowDown: "backward",
    d: "right",
    ArrowRight: "right",
  };

  const screen = document.querySelector("[data-screen]");
  const screenStatus = document.querySelector("[data-screen-status]");
  const demoStatus = document.querySelector("[data-demo-status]");
  const poseStack = document.querySelector("[data-pose-stack]");
  const launchLabel = document.querySelector("[data-launch-label]");
  const moveButtons = [...document.querySelectorAll("[data-move]")];
  let selectedPose = 2;
  let activeDirection = null;
  let launchTimer = null;

  const poseNumber = (index) => String(index + 1).padStart(2, "0");

  function renderPoses() {
    const visible = [-1, 0, 1].map((offset) => {
      const index = (selectedPose + offset + poses.length) % poses.length;
      const selected = offset === 0;
      return `<div${selected ? ' class="selected"' : ""}><span>${poseNumber(index)}</span><b>${poses[index]}</b>${selected ? "<i>›</i>" : "<i></i>"}</div>`;
    });
    poseStack.innerHTML = visible.join("");
    if (!activeDirection && !screen.classList.contains("is-launching")) {
      screenStatus.textContent = `READY // ${poses[selectedPose]}`;
    }
  }

  function changePose(amount) {
    if (screen.classList.contains("is-launching")) return;
    selectedPose = (selectedPose + amount + poses.length) % poses.length;
    renderPoses();
  }

  function runPose() {
    if (activeDirection) stopMove();
    window.clearTimeout(launchTimer);
    screen.classList.add("is-launching");
    launchLabel.textContent = `ARMING ${poses[selectedPose]}`;
    screenStatus.textContent = "ANIMATION // RUNNING";
    demoStatus.textContent = `Animating first, then sending ${poses[selectedPose].toLowerCase()}.`;
    launchTimer = window.setTimeout(() => {
      screen.classList.remove("is-launching");
      screenStatus.textContent = `SENT // ${poses[selectedPose]}`;
      demoStatus.textContent = `${poses[selectedPose]} command sent in the interface demo.`;
    }, 1200);
  }

  function startMove(direction) {
    if (!direction || screen.classList.contains("is-launching")) return;
    if (activeDirection && activeDirection !== direction) stopMove();
    activeDirection = direction;
    screen.classList.add("is-moving");
    screenStatus.textContent = `MOVING // ${direction.toUpperCase()}`;
    demoStatus.textContent = `Holding ${direction}. Release to send stop.`;
    moveButtons.forEach((button) => {
      button.classList.toggle("is-held", button.dataset.move === direction);
    });
  }

  function stopMove() {
    if (!activeDirection) return;
    const stoppedDirection = activeDirection;
    activeDirection = null;
    screen.classList.remove("is-moving");
    screenStatus.textContent = "STOPPED // RELEASED";
    demoStatus.textContent = `${stoppedDirection} released. Stop command sent in the interface demo.`;
    moveButtons.forEach((button) => button.classList.remove("is-held"));
  }

  document.querySelector("[data-pose-previous]").addEventListener("click", () => changePose(-1));
  document.querySelector("[data-pose-next]").addEventListener("click", () => changePose(1));
  document.querySelector("[data-run-pose]").addEventListener("click", runPose);

  moveButtons.forEach((button) => {
    const begin = (event) => {
      event.preventDefault();
      if (event.pointerId !== undefined) button.setPointerCapture(event.pointerId);
      startMove(button.dataset.move);
    };
    button.addEventListener("pointerdown", begin);
    button.addEventListener("pointerup", stopMove);
    button.addEventListener("pointercancel", stopMove);
    button.addEventListener("lostpointercapture", stopMove);
    button.addEventListener("keydown", (event) => {
      if ((event.key === " " || event.key === "Enter") && !event.repeat) {
        event.preventDefault();
        startMove(button.dataset.move);
      }
    });
    button.addEventListener("keyup", (event) => {
      if (event.key === " " || event.key === "Enter") {
        event.preventDefault();
        stopMove();
      }
    });
  });

  document.addEventListener("keydown", (event) => {
    const direction = directionForKey[event.key] || directionForKey[event.key.toLowerCase()];
    if (!direction || event.repeat || event.target.closest("button, a, input, textarea, select")) return;
    event.preventDefault();
    startMove(direction);
  });

  document.addEventListener("keyup", (event) => {
    const direction = directionForKey[event.key] || directionForKey[event.key.toLowerCase()];
    if (!direction || direction !== activeDirection) return;
    event.preventDefault();
    stopMove();
  });

  window.addEventListener("blur", stopMove);
  document.addEventListener("visibilitychange", () => {
    if (document.hidden) stopMove();
  });

  const copyButton = document.querySelector("[data-copy-checksum]");
  copyButton.addEventListener("click", async () => {
    const checksum = document.querySelector("[data-checksum]").textContent.trim();
    try {
      await navigator.clipboard.writeText(checksum);
      copyButton.textContent = "Copied";
    } catch {
      copyButton.textContent = "Select hash";
      window.getSelection().selectAllChildren(document.querySelector("[data-checksum]"));
    }
    window.setTimeout(() => { copyButton.textContent = "Copy"; }, 1600);
  });

  renderPoses();
})();
