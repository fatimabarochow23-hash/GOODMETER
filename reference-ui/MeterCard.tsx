import React, { useState } from 'react';
import { motion, AnimatePresence } from 'motion/react';

interface MeterCardProps {
  title: string;
  children: React.ReactNode;
  defaultExpanded?: boolean;
  indicatorColor?: string;
}

export function MeterCard({ title, children, defaultExpanded = false, indicatorColor = 'var(--accent-pink)' }: MeterCardProps) {
  const [expanded, setExpanded] = useState(defaultExpanded);

  return (
    <div className="flex flex-col w-full rounded-[8px] overflow-hidden shrink-0 border-2 border-[var(--border-color)] bg-[var(--bg-panel)] mb-[4px] shadow-[4px_4px_0px_0px_var(--border-color)] transition-all duration-200 hover:shadow-[6px_6px_0px_0px_var(--border-color)]">
      <button
        onClick={() => setExpanded(!expanded)}
        className="min-h-[48px] py-3 px-[16px] flex items-center justify-between cursor-pointer transition-all duration-150 hover:bg-[#F4F4F6] border-b-2 border-[var(--border-color)] bg-transparent"
        style={{ borderBottomWidth: expanded ? '2px' : '0px' }}
      >
        <div className="flex items-center gap-3">
          <div className="w-3.5 h-3.5 rounded-full border-2 border-[var(--border-color)]" style={{ backgroundColor: indicatorColor }} />
          <h2 className="font-[900] text-[15px] text-[var(--text-main)] uppercase tracking-[0.5px] m-0 p-0 border-none">{title}</h2>
        </div>
        <span className="text-[14px] text-[var(--text-main)] font-[800]">{expanded ? '▼' : '▶'}</span>
      </button>
      
      <AnimatePresence initial={false}>
        {expanded && (
          <motion.div
            initial={{ height: 0, opacity: 0 }}
            animate={{ height: 'auto', opacity: 1 }}
            exit={{ height: 0, opacity: 0 }}
            transition={{ duration: 0.2, ease: "easeInOut" }}
            className="overflow-hidden"
          >
            <div className="bg-[var(--bg-panel)] p-[20px]">
              {children}
            </div>
          </motion.div>
        )}
      </AnimatePresence>
    </div>
  );
}
