#!/usr/bin/env python3
"""
Implementation Plan Generator
Generates detailed implementation plan from questionnaire responses.
"""

import json
from pathlib import Path
from typing import Dict, List, Any
from datetime import datetime, timedelta

class ImplementationPlanGenerator:
    def __init__(self, responses_path: str):
        self.responses_path = Path(responses_path)
        self.responses = {}
        self.plan = {}
        
    def load_responses(self):
        """Load questionnaire responses."""
        if not self.responses_path.exists():
            raise FileNotFoundError(f"Responses file not found: {self.responses_path}")
        
        with open(self.responses_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
            self.responses = data.get('responses', {})
    
    def generate_plan(self) -> Dict[str, Any]:
        """Generate implementation plan from responses."""
        self.plan = {
            'generated_date': datetime.now().isoformat(),
            'based_on': str(self.responses_path),
            'project_vision': self._extract_vision(),
            'priorities': self._determine_priorities(),
            'phases': self._generate_phases(),
            'task_breakdown': self._generate_task_breakdown(),
            'agent_assignments': self._generate_agent_assignments(),
            'timeline': self._generate_timeline(),
            'success_criteria': self._generate_success_criteria()
        }
        
        return self.plan
    
    def _extract_vision(self) -> Dict[str, Any]:
        """Extract project vision."""
        vision = self.responses.get('project_vision', {})
        return {
            'primary_goal': vision.get('primary_goal', 'Not specified'),
            'target_timeline': vision.get('target_timeline', 'Not specified'),
            'target_audience': vision.get('target_audience', 'Not specified')
        }
    
    def _determine_priorities(self) -> Dict[str, List[str]]:
        """Determine feature priorities from responses."""
        priorities = {
            'P0': [],  # Critical
            'P1': [],  # High
            'P2': [],  # Medium
            'P3': []   # Low
        }
        
        feature_priorities = self.responses.get('feature_priorities', {})
        
        for category, features in feature_priorities.items():
            for feature_name, feature_data in features.items():
                priority_str = feature_data.get('priority', '')
                priority = self._parse_priority(priority_str)
                priorities[priority].append(f"{category}:{feature_name}")
        
        return priorities
    
    def _parse_priority(self, priority_str: str) -> str:
        """Parse priority string to P0-P3."""
        priority_str = priority_str.strip().upper()
        
        if '1' in priority_str or 'CRITICAL' in priority_str or 'P0' in priority_str:
            return 'P0'
        elif '2' in priority_str or 'HIGH' in priority_str or 'P1' in priority_str:
            return 'P1'
        elif '3' in priority_str or 'MEDIUM' in priority_str or 'P2' in priority_str:
            return 'P2'
        else:
            return 'P3'
    
    def _generate_phases(self) -> List[Dict[str, Any]]:
        """Generate development phases."""
        phases = [
            {
                'name': 'Phase 1: Foundation & Stabilization',
                'duration_weeks': 4,
                'focus': 'Build system, testing, code quality',
                'tasks': self._get_phase1_tasks()
            },
            {
                'name': 'Phase 2: Core Feature Completion',
                'duration_weeks': 8,
                'focus': 'Rendering, patterns, UI, performance',
                'tasks': self._get_phase2_tasks()
            },
            {
                'name': 'Phase 3: Polish & Advanced Features',
                'duration_weeks': 8,
                'focus': 'Advanced features, polish, release',
                'tasks': self._get_phase3_tasks()
            }
        ]
        
        return phases
    
    def _get_phase1_tasks(self) -> List[str]:
        """Get Phase 1 tasks."""
        return [
            'Build system modernization',
            'Test framework setup',
            'CI/CD pipeline',
            'Technical debt reduction',
            'Test coverage improvement',
            'Documentation standards'
        ]
    
    def _get_phase2_tasks(self) -> List[str]:
        """Get Phase 2 tasks based on priorities."""
        tasks = []
        
        priorities = self._determine_priorities()
        
        # Always include core rendering
        tasks.append('Complete 3D rendering pipeline')
        tasks.append('Camera controls')
        
        # Add based on priorities
        if any('pattern' in p.lower() for p in priorities['P0'] + priorities['P1']):
            tasks.append('Pattern management')
            tasks.append('Pattern library')
        
        if any('rule' in p.lower() for p in priorities['P0'] + priorities['P1']):
            tasks.append('Rule customization')
        
        tasks.append('UI integration')
        tasks.append('Performance optimization')
        
        return tasks
    
    def _get_phase3_tasks(self) -> List[str]:
        """Get Phase 3 tasks."""
        return [
            'Advanced rendering features',
            'Export & recording',
            'Advanced features',
            'Final polish',
            'Documentation completion',
            'Final testing'
        ]
    
    def _generate_task_breakdown(self) -> List[Dict[str, Any]]:
        """Generate detailed task breakdown."""
        tasks = []
        
        # Phase 1 tasks
        phase1_tasks = self._get_phase1_tasks()
        for i, task_name in enumerate(phase1_tasks, 1):
            tasks.append({
                'id': f'T-P1-{i:03d}',
                'name': task_name,
                'phase': 1,
                'priority': 'P0',
                'estimated_hours': 8,
                'agent': self._assign_agent(task_name),
                'dependencies': []
            })
        
        # Phase 2 tasks
        phase2_tasks = self._get_phase2_tasks()
        for i, task_name in enumerate(phase2_tasks, 1):
            tasks.append({
                'id': f'T-P2-{i:03d}',
                'name': task_name,
                'phase': 2,
                'priority': self._get_task_priority(task_name),
                'estimated_hours': 12,
                'agent': self._assign_agent(task_name),
                'dependencies': self._get_task_dependencies(task_name)
            })
        
        return tasks
    
    def _assign_agent(self, task_name: str) -> str:
        """Assign agent to task based on task name."""
        task_lower = task_name.lower()
        
        if 'render' in task_lower or 'shader' in task_lower or 'camera' in task_lower:
            return 'Graphics Specialist'
        elif 'pattern' in task_lower or 'rule' in task_lower or 'algorithm' in task_lower:
            return 'Game Logic Specialist'
        elif 'ui' in task_lower or 'integration' in task_lower or 'architecture' in task_lower:
            return 'Architecture/UI Specialist'
        elif 'build' in task_lower or 'test' in task_lower or 'ci' in task_lower:
            return 'Build/Test Specialist'
        elif 'memory' in task_lower or 'vma' in task_lower:
            return 'VMA Specialist'
        elif 'documentation' in task_lower or 'quality' in task_lower or 'debt' in task_lower:
            return 'Code Quality Specialist'
        else:
            return 'Orchestrator'
    
    def _get_task_priority(self, task_name: str) -> str:
        """Get task priority."""
        priorities = self._determine_priorities()
        task_lower = task_name.lower()
        
        for priority_level, features in priorities.items():
            if any(task_lower in f.lower() for f in features):
                return priority_level
        
        return 'P2'  # Default to medium
    
    def _get_task_dependencies(self, task_name: str) -> List[str]:
        """Get task dependencies."""
        task_lower = task_name.lower()
        dependencies = []
        
        if 'ui' in task_lower:
            dependencies.append('Complete 3D rendering pipeline')
        if 'pattern' in task_lower:
            dependencies.append('Complete 3D rendering pipeline')
        if 'performance' in task_lower:
            dependencies.append('Complete 3D rendering pipeline')
        
        return dependencies
    
    def _generate_agent_assignments(self) -> Dict[str, List[str]]:
        """Generate agent task assignments."""
        assignments = {}
        task_breakdown = self._generate_task_breakdown()
        
        for task in task_breakdown:
            agent = task['agent']
            if agent not in assignments:
                assignments[agent] = []
            assignments[agent].append(task['id'])
        
        return assignments
    
    def _generate_timeline(self) -> Dict[str, Any]:
        """Generate project timeline."""
        vision = self._extract_vision()
        timeline_str = vision.get('target_timeline', '')
        
        # Parse timeline
        if '1-3' in timeline_str or 'rapid' in timeline_str.lower():
            total_weeks = 12
        elif '3-6' in timeline_str or 'moderate' in timeline_str.lower():
            total_weeks = 20
        elif '6-12' in timeline_str or 'comprehensive' in timeline_str.lower():
            total_weeks = 40
        else:
            total_weeks = 20  # Default
        
        start_date = datetime.now()
        end_date = start_date + timedelta(weeks=total_weeks)
        
        return {
            'start_date': start_date.isoformat(),
            'end_date': end_date.isoformat(),
            'total_weeks': total_weeks,
            'phases': {
                'phase1': {'weeks': 4, 'start': start_date.isoformat()},
                'phase2': {'weeks': 8, 'start': (start_date + timedelta(weeks=4)).isoformat()},
                'phase3': {'weeks': min(8, total_weeks - 12), 'start': (start_date + timedelta(weeks=12)).isoformat()}
            }
        }
    
    def _generate_success_criteria(self) -> Dict[str, Any]:
        """Generate success criteria."""
        quality_gates = self.responses.get('quality_gates', {})
        
        return {
            'technical': {
                'test_coverage': quality_gates.get('technical', {}).get('test_coverage', '90%'),
                'static_analysis': quality_gates.get('technical', {}).get('static_analysis', '0 critical'),
                'performance_fps': quality_gates.get('technical', {}).get('minimum_fps', '60')
            },
            'process': {
                'task_completion_rate': '>90%',
                'rework_percentage': '<5%'
            },
            'features': {
                'core_features': '100%',
                'advanced_features': '80%+'
            }
        }
    
    def save_plan(self, output_path: str):
        """Save implementation plan to JSON file."""
        output_file = Path(output_path)
        output_file.parent.mkdir(parents=True, exist_ok=True)
        
        with open(output_file, 'w', encoding='utf-8') as f:
            json.dump(self.plan, f, indent=2, ensure_ascii=False)
        
        print(f"Implementation plan saved to: {output_file}")


def main():
    """Main entry point."""
    import argparse
    
    parser = argparse.ArgumentParser(description='Generate implementation plan from questionnaire responses')
    parser.add_argument('responses', help='Path to questionnaire_responses.json')
    parser.add_argument('-o', '--output', default='implementation_plan.json',
                       help='Output JSON file path')
    
    args = parser.parse_args()
    
    generator = ImplementationPlanGenerator(args.responses)
    generator.load_responses()
    plan = generator.generate_plan()
    generator.save_plan(args.output)
    
    print("\nImplementation plan generated!")
    print(f"Total tasks: {len(plan.get('task_breakdown', []))}")
    print(f"Timeline: {plan.get('timeline', {}).get('total_weeks', 'N/A')} weeks")


if __name__ == '__main__':
    main()

