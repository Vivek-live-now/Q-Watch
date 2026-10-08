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
        var rawCount = 0

        for (line in lines) {
            val trimmed = line.trim()
            if (trimmed.startsWith("#") || trimmed.isEmpty()) {
                if (currentName != null) {
                    buttons.add(
                        IrParsedButton(
                            name = currentName,
                            type = currentType ?: "parsed",
                            protocol = currentProtocol,
                            address = currentAddress,
                            command = currentCommand,
                            frequency = currentFreq,
                            rawTimingsCount = rawCount
                        )
                    )
                    currentName = null
                    currentType = null
                    currentProtocol = ""
                    currentAddress = ""
                    currentCommand = ""
                    currentFreq = 0L
                    rawCount = 0
                }
                continue
            }

            val colonIdx = trimmed.indexOf(':')
            if (colonIdx == -1) continue

            val key = trimmed.substring(0, colonIdx).trim().lowercase()
            val value = trimmed.substring(colonIdx + 1).trim()

            when (key) {
                "filetype" -> filetype = value
                "name" -> currentName = value
                "type" -> currentType = value
                "protocol" -> currentProtocol = value
                "address" -> currentAddress = value
                "command" -> currentCommand = value
                "frequency" -> currentFreq = value.toLongOrNull() ?: 0L
                "data" -> {
                    val tokens = value.split("\\s+".toRegex()).filter { it.isNotEmpty() }
                    rawCount = tokens.size
                }
            }
        }

        if (currentName != null) {
            buttons.add(
                IrParsedButton(
                    name = currentName,
                    type = currentType ?: "parsed",
                    protocol = currentProtocol,
                    address = currentAddress,
                    command = currentCommand,
                    frequency = currentFreq,
                    rawTimingsCount = rawCount
                )
            )
        }

        return IrRemoteFile(
            entry = entry,
            filetype = filetype,
            rawText = content,
            buttons = buttons
        )
    }
}
