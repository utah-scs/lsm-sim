library(ggplot2)
library(grid)
library(gridExtra)
library(quantreg)
library(reshape2)

myTheme <- theme_bw() +
  theme(panel.grid.major = element_line(color = 'darkgrey',
                                        linetype = 'dashed'),
        panel.grid.minor = element_line(color = 'lightgrey',
                                        linetype ='dotted'),
        panel.border = element_rect(color='black'),
        axis.ticks.length = unit(-0.2, 'cm'),
        axis.ticks.margin = unit(0.3, 'cm')
        #text = element_text(size = rel(3.2)),
        #legend.text = element_text(size=rel(2.5)),
        #legend.title = element_text(size=rel(2.5))
  )

bigFonts <- myTheme +
  theme(text=element_text(size=rel(5)),
        legend.text=element_text(size=rel(5)),
        legend.title=element_text(size=rel(4)))
