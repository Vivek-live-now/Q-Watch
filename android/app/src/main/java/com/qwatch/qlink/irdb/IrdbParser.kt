package com.qwatch.qlink.irdb

object IrdbParser {

    fun parse(entry: IrdbEntry, content: String): IrRemoteFile {
        val lines = content.lines()
        var filetype = "IR signals file"
        val buttons = mutableListOf<IrParsedButton>()

        var currentName: String? = null
        var currentType: String? = null
        var currentProtocol = ""
        var currentAddress = ""
        var currentCommand = ""
        var currentFreq = 0L
        var currentNbits = 0
        var currentDutyCycle = 0.33f
        val currentTimings = mutableListOf<Int>()

        fun buildCurrentButton(): IrParsedButton? {
            val name = currentName ?: return null
            val finalType = currentType ?: if (currentProtocol.isNotBlank() && !currentProtocol.equals("raw", ignoreCase = true)) "parsed" else "raw"
            val effectiveNbits = if (currentNbits > 0) {
                currentNbits
            } else if (currentProtocol.isNotBlank()) {
                IrParsedButton.getDefaultBitsForProtocol(currentProtocol)
            } else {
                0
            }
            return IrParsedButton(
                name = name,
                type = finalType,
                protocol = currentProtocol,
                address = currentAddress,
                command = currentCommand,
                frequency = currentFreq,
                rawTimingsCount = currentTimings.size,
                nbits = effectiveNbits,
                rawTimings = currentTimings.toList(),
                dutyCycle = currentDutyCycle
            )
        }

        fun resetCurrentButton() {
            currentName = null
            currentType = null
            currentProtocol = ""
            currentAddress = ""
            currentCommand = ""
            currentFreq = 0L
            currentNbits = 0
            currentDutyCycle = 0.33f
            currentTimings.clear()
        }

        for (line in lines) {
            val trimmed = line.trim()
            if (trimmed.startsWith("#") || trimmed.isEmpty()) {
                if (currentName != null) {
                    buildCurrentButton()?.let { buttons.add(it) }
                    resetCurrentButton()
                }
                continue
            }

            val colonIdx = trimmed.indexOf(':')
            if (colonIdx == -1) continue

            val key = trimmed.substring(0, colonIdx).trim().lowercase()
            val value = trimmed.substring(colonIdx + 1).trim()

            when (key) {
                "filetype" -> filetype = value
                "name" -> {
                    if (currentName != null) {
                        buildCurrentButton()?.let { buttons.add(it) }
                        resetCurrentButton()
                    }
                    currentName = value
                }
                "type" -> currentType = value
                "protocol" -> currentProtocol = value
                "address" -> currentAddress = value
                "command" -> currentCommand = value
                "nbits", "bits" -> {
                    currentNbits = value.toIntOrNull()?.takeIf { it > 0 } ?: 0
                }
                "frequency" -> currentFreq = value.toLongOrNull() ?: 0L
                "duty_cycle" -> currentDutyCycle = value.toFloatOrNull() ?: 0.33f
                "data" -> {
                    val tokens = value.split("\\s+".toRegex()).filter { it.isNotEmpty() }
                    for (token in tokens) {
                        val t = token.toIntOrNull()
                        if (t != null && t > 0) {
                            currentTimings.add(t)
                        }
                    }
                }
            }
        }

        if (currentName != null) {
            buildCurrentButton()?.let { buttons.add(it) }
        }

        return IrRemoteFile(
            entry = entry,
            filetype = filetype,
            rawText = content,
            buttons = buttons
        )
    }
}
